<?php

/**
 * @file
 * OpenAI Content Lifecycle Analyzer class for Backdrop CMS.
 */

require_once BACKDROP_ROOT . '/' . backdrop_get_path('module', 'openai') . '/vendor/autoload.php';

/**
 * Service for performing AI content analysis on entities.
 */
class OpenAIContentLifecycleAnalyzer {

  /**
   * Default system prompt for content analysis.
   */
  protected $technicalSystemPrompt = '';

  /**
   * The OpenAI API client.
   *
   * @var OpenAIApi
   */
  protected $openaiApi;

  /**
   * Constructor.
   */
  public function __construct() {
    // Get API key from config.
    $api_key_config = config_get('openai.settings', 'api_key');
    $api_key = NULL;
    
    if (module_exists('key') && !empty($api_key_config)) {
      $api_key = key_get_key_value($api_key_config);
    }
    
    if (empty($api_key)) {
      watchdog('openai_content_lifecycle', 'OpenAI API key not configured.', array(), WATCHDOG_ERROR);
      return;
    }
    
    $this->openaiApi = new OpenAIApi($api_key);
  }

  /**
   * Analyzes content using AI and returns the analysis results.
   *
   * @param object $entity
   *   The entity to analyze.
   *
   * @return string|null
   *   The AI analysis results, or NULL if content doesn't need review.
   */
  public function analyzeContent($entity) {
    $config = config('openai_content_lifecycle.settings');

    // Extract content from entity.
    $content = $this->extractEntityContent($entity);

    // Get prompt based on entity type/bundle.
    $entity_prompt = $this->getPromptForEntity($entity);
    $pre_prompt = $config->get('pre_prompt');
    if (empty($pre_prompt)) {
      $pre_prompt = $this->getDefaultSystemPrompt();
    }
    
    $total_prompt = $pre_prompt . $this->technicalSystemPrompt;
    $prompt = str_replace('[conditions]', $entity_prompt, $total_prompt);

    // Get current date in d.m.Y format.
    $date = format_date(REQUEST_TIME, 'custom', 'd.m.Y');
    $prompt = str_replace('[ai_content_lifecycle:date]', $date, $prompt);
    
    $language = language_default();
    $prompt = str_replace('[lang]', $language->name, $prompt);
    $prompt = str_replace('[context]', $content['title'] . "\n\n" . $content['content'], $prompt);

    // Allow other modules to alter the prompt.
    backdrop_alter('openai_content_lifecycle_prompt', $prompt, $entity);

    // Prepare messages for OpenAI.
    $messages = array(
      array(
        'role' => 'system',
        'content' => 'You are an agent that responds with only XTRUE or XFALSE. You evaluate CONTEXT based on the INSTRUCTION the user provides. You return (XTRUE) if the INSTRUCTION provided indicate that the content should be updated or (XFALSE) if not.',
      ),
      array(
        'role' => 'user',
        'content' => $prompt,
      ),
    );

    $model = $config->get('default_model');
    if (empty($model)) {
      $model = 'gpt-4o-mini'; // Default model
    }
    
    $temperature = $config->get('temperature');
    if (empty($temperature) && $temperature !== 0) {
      $temperature = 0.7;
    }
    // Ensure temperature is a float.
    $temperature = floatval($temperature);

    try {
      // Call OpenAI API.
      $response = $this->openaiApi->chat($model, $messages, $temperature, 500, FALSE);
      
      if (empty($response)) {
        return NULL;
      }
      
      $result = is_string($response) ? $response : (isset($response->content) ? $response->content : '');
      
      if (stripos($result, 'XTRUE') !== FALSE) {
        // Content needs review, get detailed explanation.
        $subPrompt = "
VARIABLES
----------
Today is [ai_content_lifecycle:date]

INSTRUCTION
-----------
The below content was marked for review (context).
The content should be marked when
[conditions]

Describe in [lang] in one or two sentences which sentences triggered you to mark this for reviewing always referring to the conditions above.

CONTEXT
-------
[context]
";
        $subPrompt = str_replace('[ai_content_lifecycle:date]', $date, $subPrompt);
        $subPrompt = str_replace('[lang]', $language->name, $subPrompt);
        $subPrompt = str_replace('[context]', $content['title'] . "\n\n" . $content['content'], $subPrompt);
        $subPrompt = str_replace('[conditions]', $entity_prompt, $subPrompt);
        
        $messages2 = array(
          array(
            'role' => 'system',
            'content' => 'You are a content evaluator, helping web editors. You describe why content should be updated.',
          ),
          array(
            'role' => 'user',
            'content' => $subPrompt,
          ),
        );
        
        $response2 = $this->openaiApi->chat($model, $messages2, $temperature, 500, FALSE);
        return is_string($response2) ? $response2 : (isset($response2->content) ? $response2->content : '');
      }
      
      return NULL;
    }
    catch (Exception $e) {
      watchdog('openai_content_lifecycle', 'Error analyzing content with AI: @message', array('@message' => $e->getMessage()), WATCHDOG_ERROR);
      return NULL;
    }
  }

  /**
   * Gets the appropriate prompt for the entity based on its type and bundle.
   *
   * @param object $entity
   *   The entity to get the prompt for.
   *
   * @return string
   *   The prompt to use for content analysis.
   */
  protected function getPromptForEntity($entity) {
    $config = config('openai_content_lifecycle.settings');
    $bundle_prompts = $config->get('bundle_prompts');
    if (empty($bundle_prompts)) {
      $bundle_prompts = array();
    }

    $entity_type = $entity->entityType();
    $bundle = isset($entity->type) ? $entity->type : $entity_type;

    // Try to get bundle-specific prompt.
    if (isset($bundle_prompts[$entity_type][$bundle])) {
      return is_array($bundle_prompts[$entity_type][$bundle])
        ? $bundle_prompts[$entity_type][$bundle]['value']
        : $bundle_prompts[$entity_type][$bundle];
    }

    // Try entity-type prompt.
    if (isset($bundle_prompts[$entity_type]['default'])) {
      return is_array($bundle_prompts[$entity_type]['default'])
        ? $bundle_prompts[$entity_type]['default']['value']
        : $bundle_prompts[$entity_type]['default'];
    }

    // Default prompt from configuration.
    $default = $config->get('default_prompt');
    if (empty($default)) {
      $default = $this->getDefaultPrompt();
    }
    return $default;
  }

  /**
   * Extracts relevant content from an entity for AI analysis.
   *
   * @param object $entity
   *   The entity to extract content from.
   *
   * @return array
   *   The extracted content with 'title' and 'content' keys.
   */
  protected function extractEntityContent($entity) {
    $content = array();
    $config = config('openai_content_lifecycle.settings');
    
    // Get entity label/title.
    if (isset($entity->title)) {
      $content['title'] = $entity->title;
    }
    elseif (isset($entity->name)) {
      $content['title'] = $entity->name;
    }
    else {
      $content['title'] = '';
    }

    // Allow other modules to prepare content.
    $content['content'] = '';
    foreach (module_implements('openai_content_lifecycle_prepare_content') as $module) {
      $function = $module . '_openai_content_lifecycle_prepare_content';
      $result = $function($entity);
      if (!empty($result)) {
        $content['content'] .= $result . "\n\n";
      }
    }

    // If no content was prepared, extract from fields.
    if (empty(trim($content['content']))) {
      $content['content'] = $this->extractTextFieldsContent($entity);
    }

    // Clean up content.
    $content['content'] = trim(preg_replace('/\n\s*\n/', "\n", $content['content']));

    // Get updated date if available.
    $content['updated'] = NULL;
    if (isset($entity->changed)) {
      $content['updated'] = format_date($entity->changed, 'medium');
    }

    // Get language.
    $default_language = language_default();
    $content['language'] = $default_language->langcode;
    if (isset($entity->langcode)) {
      $content['language'] = $entity->langcode;
    }

    // Allow other modules to alter the content.
    backdrop_alter('openai_content_lifecycle_content', $content, $entity);

    return $content;
  }

  /**
   * Extract content from text fields as a fallback method.
   *
   * @param object $entity
   *   The entity to extract content from.
   *
   * @return string
   *   The extracted text content.
   */
  protected function extractTextFieldsContent($entity) {
    $content = '';
    
    // For nodes, get body and other text fields.
    if (isset($entity->type)) {
      $fields = field_info_instances($entity->entityType(), $entity->type);
      
      foreach ($fields as $field_name => $instance) {
        $field_info = field_info_field($field_name);
        $field_type = $field_info['type'];
        
        // Check if this is a text field type.
        if (in_array($field_type, array('text', 'text_long', 'text_with_summary'))) {
          $items = field_get_items($entity->entityType(), $entity, $field_name);
          
          if ($items) {
            foreach ($items as $item) {
              if (isset($item['value'])) {
                $content .= strip_tags($item['value']) . "\n\n";
              }
            }
          }
        }
      }
    }

    return $content;
  }

  /**
   * Get the default system prompt.
   *
   * @return string
   *   The default system prompt.
   */
  protected function getDefaultSystemPrompt() {
    return "
Format:
------
Respond XTRUE or XFALSE, nothing else, no pleasantries or other things.

Variables:
---------
Today is [ai_content_lifecycle:date]

INSTRUCTIONS
------------
return XTRUE when the content
[conditions]

Otherwise return XFALSE

This is the content you have evaluate:
----------------------------------------
[context]
";
  }

  /**
   * Get the default prompt conditions.
   *
   * @return string
   *   The default prompt conditions.
   */
  protected function getDefaultPrompt() {
    return "- contains inconsistencies or contradictions\n- appears to be outdated and a human should check it";
  }

  /**
   * Updates a ContentLifecycle entity with AI analysis results.
   *
   * @param object $entity
   *   The entity to analyze.
   * @param ContentLifecycle|null $lifecycle
   *   Optional lifecycle entity to update, or NULL to find/create one.
   *
   * @return ContentLifecycle|null
   *   The updated lifecycle entity or NULL on failure.
   */
  public function updateContentLifecycleWithAnalysis($entity, $lifecycle = NULL) {
    // Find existing or create new lifecycle entity.
    if (!$lifecycle) {
      $existing = ContentLifecycle::findForEntity($entity);
      
      if (!empty($existing)) {
        $lifecycle = reset($existing);
      }
      else {
        $lifecycle = entity_create('content_lifecycle', array());
        $lifecycle->setReferencedEntity($entity);
      }
    }
    
    // Get AI analysis.
    $analysis_result = $this->analyzeContent($entity);
    
    // Update the AI evaluation results.
    if ($analysis_result) {
      $lifecycle->ai_results = $analysis_result;
      $lifecycle->review_status = 'analyzed';
      $lifecycle->save();
      
      return $lifecycle;
    }
    
    return NULL;
  }

}
