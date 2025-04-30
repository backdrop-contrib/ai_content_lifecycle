<?php

namespace Drupal\ai_content_lifecycle;

use Drupal\ai\AiProviderPluginManager;
use Drupal\ai\OperationType\Chat\ChatInput;
use Drupal\ai\OperationType\Chat\ChatMessage;
use Drupal\ai_content_lifecycle\Form\ContentLifecycleSettingsForm;
use Drupal\Core\Datetime\DateFormatterInterface;
use Drupal\Core\Entity\EntityInterface;
use Drupal\Core\Config\ConfigFactoryInterface;
use Drupal\Core\Extension\ModuleHandlerInterface;
use Drupal\Core\Language\LanguageManagerInterface;
use Drupal\Core\Logger\LoggerChannelFactoryInterface;
use Drupal\Core\Render\RendererInterface;
use Drupal\Core\StringTranslation\StringTranslationTrait;
use Drupal\Core\Entity\EntityTypeManagerInterface;
use League\HTMLToMarkdown\HtmlConverter;

/**
 * Service for performing AI content analysis on entities.
 */
class ContentAIAnalyzer {

  use StringTranslationTrait;

  /**
   * Default system prompt for content analysis.
   *
   * @var string
   */
  protected $technicalSystemPrompt = '

  ';

  /**
   * The constructor.
   *
   * @param \Drupal\Core\Config\ConfigFactoryInterface $configFactory
   *   The config factory.
   * @param \Drupal\Core\Entity\EntityTypeManagerInterface $entityTypeManager
   *   The entity type manager.
   * @param \Drupal\ai\AiProviderPluginManager $aiProvider
   *   The AI provider plugin manager.
   * @param \Drupal\Core\Render\RendererInterface $renderer
   *   The renderer service.
   * @param \Drupal\Core\Language\LanguageManagerInterface $languageManager
   *   The language manager service.
   * @param \Drupal\Core\Logger\LoggerChannelFactoryInterface $loggerFactory
   *   The logger channel factory service.
   * @param \Drupal\Core\Datetime\DateFormatterInterface $dateFormatter
   *   The date formatter service.
   */
  public function __construct(
    protected ConfigFactoryInterface $configFactory,
    protected EntityTypeManagerInterface $entityTypeManager,
    protected AiProviderPluginManager $aiProvider,
    protected RendererInterface $renderer,
    protected LanguageManagerInterface $languageManager,
    protected LoggerChannelFactoryInterface $loggerFactory,
    protected DateFormatterInterface $dateFormatter,
    protected ModuleHandlerInterface $moduleHandler,
  ) {
  }

  /**
   * Analyzes content using AI and returns the analysis results.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to analyze.
   *
   * @return string
   *   The AI analysis results.
   */
  public function analyzeContent(EntityInterface $entity) {
    $config = $this->configFactory->get('ai_content_lifecycle.settings');

    // Extract content from entity
    $content = $this->extractEntityContent($entity);

    // Get prompt based on entity type/bundle
    $prompt = $this->getPromptForEntity($entity);
    $pre_prompt = $config->get('pre_prompt');
    $total_prompt = $pre_prompt . $this->technicalSystemPrompt;
    $prompt = str_replace('[conditions]', $prompt, $total_prompt);

    // Get curren date in dmY format
    $date = $this->dateFormatter->format(time(), 'custom', 'd.m.Y');
    $prompt = str_replace('[ai_content_lifecycle:date]', $date, $prompt);

    $prompt = str_replace('[lang]', $this->languageManager->getCurrentLanguage()->getName(), $prompt);

    // Allow overriding of the prompt in other modules (like tone of voice or tokens).
    $this->moduleHandler->alter('ai_content_lifecycle_prompt', $prompt);

    // Create the chat messages with date in dmY and content in triple quotes
    $messages = new ChatInput([
      new ChatMessage('system', $prompt),
      new ChatMessage('user', json_encode($content)),
    ]);

    $config_model = $config->get('default_model');
    if ($config_model == NULL) {
      // Get the default AI provider for chat operations
      $sets = $this->aiProvider->getDefaultProviderForOperationType('chat');
    } else {

      // Get the overridden AI provider for chat operations
      $parts = explode('__', $config_model);
      $sets['model_id'] = $parts[1];
      $sets['provider_id'] = $parts[0];
    }

    try {
      // Create provider instance and send the chat request
      $provider = $this->aiProvider->createInstance($sets['provider_id']);
      $response = $provider->chat($messages, $sets['model_id'])->getNormalized();

      $result = $response->getText();

      // Parse the JSON response
      $json = json_decode($result, TRUE);

      // If valid JSON and outdated is true, return the reason
      if (is_array($json) && isset($json['mark_for_update'])) {
        if (($json['mark_for_update'] === 'true' || $json['mark_for_update'] === TRUE) && !empty($json['reason'])) {
          return $json['reason'];
        }
        else {
          // Content is not outdated
          return NULL;
        }
      }

      // If valid JSON and outdated is true, return the reason
      if (is_array($json) && isset($json['mark_for_update']) && $json['mark_for_update'] === 'TRUE' && !empty($json['reason'])) {
        return $json['reason'];
      }

      // If not outdated or invalid format, return null
      return NULL;
    }
    catch (\Exception $e) {
      $this->loggerFactory->get('ai_content_lifecycle')->error(
        'Error analyzing content with AI: @message', ['@message' => $e->getMessage()]
      );
      return NULL;
    }
  }

  /**
   * Gets the appropriate prompt for the entity based on its type and bundle.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to get the prompt for.
   *
   * @return string
   *   The prompt to use for content analysis.
   */
  protected function getPromptForEntity(EntityInterface $entity) {
    $config = $this->configFactory->get('ai_content_lifecycle.settings');
    $bundle_prompts = $config->get('bundle_prompts') ?: [];

    $entity_type_id = $entity->getEntityTypeId();
    $bundle = method_exists($entity, 'bundle') ? $entity->bundle() : $entity->getEntityTypeId();

    // Try to get bundle-specific prompt
    if (isset($bundle_prompts[$entity_type_id][$bundle])) {
      return is_array($bundle_prompts[$entity_type_id][$bundle])
        ? $bundle_prompts[$entity_type_id][$bundle]['value']
        : $bundle_prompts[$entity_type_id][$bundle];
    }

    // Try entity-type prompt
    if (isset($bundle_prompts[$entity_type_id]['default'])) {
      return is_array($bundle_prompts[$entity_type_id]['default'])
        ? $bundle_prompts[$entity_type_id]['default']['value']
        : $bundle_prompts[$entity_type_id]['default'];
    }

    // Default prompt from configuration
    return $config->get('default_prompt') ?: ContentLifecycleSettingsForm::DEFAULT_SYSTEM_PROMPT;
  }

  /**
   * Extracts relevant content from an entity for AI analysis.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to extract content from.
   *
   * @return array
   *   The extracted content.
   */
  protected function extractEntityContent(EntityInterface $entity) {
    $content = [];

    $converter = new HtmlConverter(['strip_tags' => TRUE]);
    $entity_type_id = $entity->getEntityTypeId();

    // Get configured view mode or fall back to search_index
    $config = $this->configFactory->get('ai_content_lifecycle . settings');
    $view_modes = $config->get('view_modes') ?: [];
    $view_mode = $view_modes[$entity_type_id] ?? 'search_index';

    // Add title/label for any entity type
    $content['title'] = $entity->label();

    try {
      // Try to render the entity in the configured view mode
      $view_builder = $this->entityTypeManager->getViewBuilder($entity_type_id);
      $build = $view_builder->view($entity, $view_mode);
      $rendered = $this->renderer->renderInIsolation($build);
      $content['content'] = $converter->convert($rendered->__toString());
    }
    catch (\Exception $e) {
      // Fallback to text fields if rendering fails
      $content['content'] = $converter->convert($this->extractTextFieldsContent($entity));
    }

    // Add updated date if available
    if ($entity->hasField('updated')) {
      $date = $this->dateFormatter->format($entity->get('updated')->value, 'medium');
      $content['updated'] = $date;
    }

    // Add the language if entity is translatable, else fallback to default system language
    $default_language = $this->languageManager->getDefaultLanguage()->getId();
    $content['language'] = $default_language;

    if ($entity->hasField('langcode')) {
      $language = $entity->get('langcode')->value;
      $content['language'] = $language;
    }

    return $content;
  }

  /**
   * Gets text fields of an entity.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to get text fields from.
   *
   * @return array
   *   An array of text fields.
   */
  protected function getTextFields(EntityInterface $entity) {
    $text_fields = [];

    foreach ($entity->getFieldDefinitions() as $field_name => $field_definition) {
      $field_type = $field_definition->getType();
      if (in_array($field_type, ['text', 'text_long', 'text_with_summary', 'string', 'string_long'])) {
        $text_fields[$field_name] = $field_definition;
      }
    }

    return $text_fields;
  }

  /**
   * Extract content from text fields as a fallback method.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to extract content from.
   *
   * @return string
   *   The extracted text content.
   */
  protected function extractTextFieldsContent(EntityInterface $entity) {
    $content = '';
    $text_fields = $this->getTextFields($entity);

    foreach ($text_fields as $field_name => $field_definition) {
      if (!$entity->get($field_name)->isEmpty()) {
        $field_label = $field_definition->getLabel();
        $field_items = $entity->get($field_name);

        $field_content = '';
        foreach ($field_items as $item) {
          $value = $item->getValue();
          if (isset($value['value'])) {
            $field_content .= strip_tags($value['value']) . "\n";
          }
          elseif (is_string($value)) {
            $field_content .= strip_tags($value) . "\n";
          }
        }

        if (!empty($field_content)) {
          $content .= "$field_label: $field_content\n\n";
        }
      }
    }

    return $content;
  }

  /**
   * Updates a ContentLifeCycle entity with AI analysis results.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to analyze.
   * @param \Drupal\ai_content_lifecycle\Entity\ContentLifeCycle|null $lifecycle
   *   The ContentLifeCycle entity to update, or NULL to find/create one.
   *
   * @return \Drupal\ai_content_lifecycle\Entity\ContentLifeCycle
   *   The updated ContentLifeCycle entity.
   */
  public function updateContentLifecycleWithAnalysis(EntityInterface $entity, $lifecycle = NULL) {
    // Find existing or create new lifecycle entity
    if (!$lifecycle) {
      $existing = \Drupal\ai_content_lifecycle\Entity\ContentLifeCycle::findForEntity($entity);

      if (!empty($existing)) {
        $lifecycle = reset($existing);
      }
      else {
        $lifecycle = $this->entityTypeManager->getStorage('content_life_cycle')->create([
          'label' => $entity->label() ?? 'Lifecycle for ' . $entity->getEntityTypeId() . ':' . $entity->id(),
        ]);
        $lifecycle->setReferencedEntity($entity);
      }
    }

    // Get AI analysis
    $analysis_result = $this->analyzeContent($entity);

    // Update the AI evaluation results field
    $lifecycle->set('ai_prompt_results', $analysis_result);
    $lifecycle->save();

    return $lifecycle;
  }

}
