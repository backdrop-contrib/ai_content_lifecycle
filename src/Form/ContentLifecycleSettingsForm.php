<?php

namespace Drupal\ai_content_lifecycle\Form;

use Drupal\ai\AiProviderPluginManager;
use Drupal\Core\Config\ConfigFactoryInterface;
use Drupal\Core\Config\TypedConfigManagerInterface;
use Drupal\Core\Entity\EntityTypeBundleInfoInterface;
use Drupal\Core\Entity\EntityTypeManagerInterface;
use Drupal\Core\Form\ConfigFormBase;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Url;
use Symfony\Component\DependencyInjection\ContainerInterface;

/**
 * Configure AI Content Lifecycle settings.
 */
class ContentLifecycleSettingsForm extends ConfigFormBase {

  /**
   * Config settings.
   *
   * @var string
   */
  const SETTINGS = 'ai_content_lifecycle.settings';

  public const DEFAULT_SYSTEM_PROMPT = '
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
  ';

  /**
   * The entity type manager.
   *
   * @var \Drupal\Core\Entity\EntityTypeManagerInterface
   */
  protected $entityTypeManager;

  /**
   * The entity type bundle info.
   *
   * @var \Drupal\Core\Entity\EntityTypeBundleInfoInterface
   */
  protected $entityTypeBundleInfo;

  /**
   * The AI provider manager.
   *
   * @var \Drupal\ai\AiProviderPluginManager
   */
  protected $aiProviderManager;

  /**
   * The example prompt.
   *
   * @var string
   */
  protected $example;

  /**
   * Constructor for ContentLifecycleSettingsForm.
   *
   * @param \Drupal\Core\Config\ConfigFactoryInterface $config_factory
   *   The config factory.
   * @param \Drupal\Core\Config\TypedConfigManagerInterface $typedConfigManager
   *   The typed config manager.
   * @param \Drupal\Core\Entity\EntityTypeManagerInterface $entity_type_manager
   *   The entity type manager.
   * @param \Drupal\Core\Entity\EntityTypeBundleInfoInterface $entity_type_bundle_info
   *   The entity type bundle info.
   */
  public function __construct(
    ConfigFactoryInterface $config_factory,
    TypedConfigManagerInterface $typedConfigManager,
    EntityTypeManagerInterface $entity_type_manager,
    EntityTypeBundleInfoInterface $entity_type_bundle_info,
    AiProviderPluginManager $aiProviderManager,
  ) {
    parent::__construct($config_factory, $typedConfigManager);
    $this->entityTypeManager = $entity_type_manager;
    $this->entityTypeBundleInfo = $entity_type_bundle_info;
    $this->aiProviderManager = $aiProviderManager;
    $this->example = $this->t('- it mentions the queen of england. '
      . PHP_EOL . '- it mentions the king of Germany.'
      . PHP_EOL . '- it contains inconsistencies or contradictions'
      . PHP_EOL . '- you are really sure that it is outdated and a human should check it');
  }

  /**
   * {@inheritdoc}
   */
  public static function create(ContainerInterface $container) {
    return new static(
      $container->get('config.factory'),
      $container->get('config.typed'),
      $container->get('entity_type.manager'),
      $container->get('entity_type.bundle.info'),
      $container->get('ai.provider')
    );
  }

  /**
   * {@inheritdoc}
   */
  protected function getEditableConfigNames() {
    return [
      static::SETTINGS,
    ];
  }

  /**
   * {@inheritdoc}
   */
  public function getFormId() {
    return 'ai_content_lifecycle_settings';
  }

  /**
   * {@inheritdoc}
   */
  public function buildForm(array $form, FormStateInterface $form_state) {
    $config = $this->config(static::SETTINGS);

    // Ai configuration hidden below.
    $form['ai_settings'] = [
      '#title' => $this->t('Advanced LLM settings'),
      '#type' => 'details',
      '#tree' => TRUE,
      '#open' => FALSE,
      '#weight' => 4,
    ];

    // Model selection.
    $llm_model_options = $this->aiProviderManager->getSimpleProviderModelOptions('chat');
    array_shift($llm_model_options);
    $form['ai_settings']['default_model'] = [
      '#type' => 'select',
      "#empty_option" => $this->t('-- Default from AI module (chat) --'),
      '#title' => $this->t('LLM to use for content evaluation.'),
      '#default_value' => $config->get('default_model'),
      '#options' => $llm_model_options,
      '#description' => $this->t('Select which provider to use for this plugin. See the <a href=":link">Provider overview</a> for details about each provider.', [':link' => '/admin/config/ai/providers']),
    ];

    // Pre prompt.
    $form['ai_settings']['pre_prompt'] = [
      '#type' => 'textarea',
      '#rows' => 25,
      '#title' => $this->t('Pre prompt'),
      '#default_value' => $config->get('pre_prompt') ?: static::DEFAULT_SYSTEM_PROMPT,
      '#description' => $this->t('The prompt used for marking content. you can use the following tokens:
      <br><b>[conditions]</b> (the reasons mentioned above why content might be requiring an update)
      <br><b>[ai_content_lifecycle:date]</b> The current date
      <br><b>[lang]</b> The current language of the admin interface'),
    ];

    // Things to mark for updating.
    $form['default_prompt'] = [
      '#type' => 'textarea',
      '#title' => $this->t('Mark my content when'),
      '#default_value' => $config->get('default_prompt'),
      '#description' => $this->t('Type something like "it mentions the old tax rate of 19% which is now 21%."'),
      '#rows' => 4,
      '#placeholder' => $this->example,
    ];

    $form['description'] = [
      '#markup' => $this->t('<p>Select which content entities should be checked by the AI Content Lifecycle system.</p>'),
    ];

    // Get all content entity types that have bundles.
    $content_entity_types = $this->getAvailableContentEntityTypes();

    $form['entity_types'] = [
      '#type' => 'details',
      '#title' => $this->t('What to check'),
      '#tree' => TRUE,
      '#open' => FALSE,
    ];

    // Get saved settings.
    $enabled_entity_types = $config->get('enabled_entity_types') ?: [];
    $enabled_bundles = $config->get('enabled_bundles') ?: [];
    $bundle_prompts = $config->get('bundle_prompts') ?: [];
    $view_modes = $config->get('view_modes') ?: [];

    // For each content entity type, let admin choose which bundles to enable.
    foreach ($content_entity_types as $entity_type_id => $entity_type) {
      $form['entity_types'][$entity_type_id] = [
        '#type' => 'details',
        '#title' => $entity_type->getLabel(),
        '#open' => !empty($enabled_entity_types[$entity_type_id]),
      ];

      $form['entity_types'][$entity_type_id]['enabled'] = [
        '#type' => 'checkbox',
        '#title' => $this->t('Check contents of @type', ['@type' => $entity_type->getLabel()]),
        '#default_value' => $enabled_entity_types[$entity_type_id] ?? FALSE,
      ];

      // Add view mode selection.
      $view_mode_options = $this->getViewModeOptions($entity_type_id);
      if (!empty($view_mode_options)) {
        $form['entity_types'][$entity_type_id]['view_mode'] = [
          '#type' => 'select',
          '#title' => $this->t('View mode for content extraction'),
          '#options' => $view_mode_options,
          '#default_value' => $view_modes[$entity_type_id] ?? 'search_index',
          '#required' => TRUE,
          '#description' => $this->t('Select which view mode to use when extracting content for AI analysis.'),
          '#states' => [
            'visible' => [
              ':input[name="entity_types[' . $entity_type_id . '][enabled]"]' => ['checked' => TRUE],
            ],
          ],
        ];
      }

      // Get all bundles for the entity type.
      $bundles = $this->entityTypeBundleInfo->getBundleInfo($entity_type_id);

      if (!empty($bundles)) {
        $bundle_options = [];
        foreach ($bundles as $bundle_id => $bundle_info) {
          $bundle_options[$bundle_id] = $bundle_info['label'];
        }

        $form['entity_types'][$entity_type_id]['bundles'] = [
          '#type' => 'checkboxes',
          '#title' => $this->t('Bundles to check'),
          '#options' => $bundle_options,
          '#default_value' => $enabled_bundles[$entity_type_id] ?? [],
          '#states' => [
            'visible' => [
              ':input[name="entity_types[' . $entity_type_id . '][enabled]"]' => ['checked' => TRUE],
            ],
          ],
        ];

        // Add bundle-specific prompt configurations
        $form['entity_types'][$entity_type_id]['prompt_config'] = [
          '#type' => 'details',
          '#title' => $this->t('Prompt specifically for @type', ['@type' => $entity_type->getLabel()]),
          '#open' => FALSE,
          '#states' => [
            'visible' => [
              ':input[name="entity_types[' . $entity_type_id . '][enabled]"]' => ['checked' => TRUE],
            ],
          ],
        ];

        foreach ($bundles as $bundle_id => $bundle_info) {
          $prompt_id = $entity_type_id . '_' . $bundle_id;
          $form['entity_types'][$entity_type_id]['prompt_config'][$prompt_id] = [
            '#type' => 'textarea',
            '#title' => $this->t('Mark @bundle when', ['@bundle' => $bundle_info['label']]),
            '#default_value' => is_array($bundle_prompts[$entity_type_id][$bundle_id] ?? NULL)
              ? ($bundle_prompts[$entity_type_id][$bundle_id]['value'] ?? '')
              : ($bundle_prompts[$entity_type_id][$bundle_id] ?? ''),
            '#description' => $this->t('AI prompt template for content lifecycle checks for @bundle items. Use [entity:field_name] tokens to include entity field values.',
              ['@bundle' => $bundle_info['label']]),
            '#placeholder' => $this->example,
            '#rows' => 6,
            '#states' => [
              'visible' => [
                ':input[name="entity_types[' . $entity_type_id . '][bundles][' . $bundle_id . ']"]' => ['checked' => TRUE],
              ],
            ],
          ];
        }
      }
      else {
        $form['entity_types'][$entity_type_id]['no_bundles'] = [
          '#markup' => $this->t('This entity type does not have bundles.'),
        ];

        // For entity types without bundles, add a single prompt configuration
        $prompt_id = $entity_type_id;
        $form['entity_types'][$entity_type_id]['prompt_config'] = [
          '#type' => 'text_format',
          '#title' => $this->t('Prompt input for @type', ['@type' => $entity_type->getLabel()]),
          '#default_value' => $bundle_prompts[$entity_type_id]['default']['value'] ?? '',
          '#format' => $bundle_prompts[$entity_type_id]['default']['format'] ?? 'plain_text',
          '#description' => $this->t('AI prompt template for content lifecycle checks. Use [entity:field_name] tokens to include entity field values.'),
          '#rows' => 6,
          '#states' => [
            'visible' => [
              ':input[name="entity_types[' . $entity_type_id . '][enabled]"]' => ['checked' => TRUE],
            ],
          ],
        ];
      }
    }

    // Add batch create button as a submit button
    $form['actions']['batch_create'] = [
      '#type' => 'submit',
      '#value' => $this->t('Analyze content'),
      '#button_type' => 'primary',
      '#submit' => ['::submitForm', '::redirectToConfirm'],
      '#weight' => 5,
    ];

    return parent::buildForm($form, $form_state);
  }

  /**
   * {@inheritdoc}
   */
  public function submitForm(array &$form, FormStateInterface $form_state) {
    $config = $this->config(static::SETTINGS);
    $content_entity_types = $this->getAvailableContentEntityTypes();

    // Save the default prompt
    $config->set('default_prompt', $form_state->getValue('default_prompt'));
    $config->set('default_model', $form_state->getValue('ai_settings')['default_model']);
    $config->set('pre_prompt', $form_state->getValue('ai_settings')['pre_prompt']);

    $enabled_entity_types = [];
    $enabled_bundles = [];
    $bundle_prompts = [];
    $view_modes = [];

    // Get all values from the form
    $values = $form_state->getValues();
    $entity_types = $values['entity_types'];

    foreach (array_keys($content_entity_types) as $entity_type_id) {
      // Check if this entity type section exists in the submitted form values.
      if (isset($entity_types[$entity_type_id]['enabled']) && $entity_types[$entity_type_id]['enabled']) {
        $enabled_entity_types[$entity_type_id] = TRUE;

        // Check if this entity type has bundles.
        if (isset($entity_types[$entity_type_id]['bundles'])) {
          $enabled_bundles[$entity_type_id] = array_filter($entity_types[$entity_type_id]['bundles']);

          // Save prompts for each bundle.
          foreach (array_keys(array_filter($entity_types[$entity_type_id]['bundles'])) as $bundle_id) {
            $prompt_id = $entity_type_id . '_' . $bundle_id;
            if (isset($entity_types[$entity_type_id]['prompt_config'][$prompt_id])) {
              $bundle_prompts[$entity_type_id][$bundle_id] = $entity_types[$entity_type_id]['prompt_config'][$prompt_id];
            }
            if (isset($entity_types[$entity_type_id]['view_mode'])) {
              $view_modes[$entity_type_id][$bundle_id] = $entity_types[$entity_type_id]['view_mode'];
            }
          }
        }
        else {
          // For entity types without bundles.
          if (isset($entity_types[$entity_type_id]['prompt_config'])) {
            $bundle_prompts[$entity_type_id]['default'] = $entity_types[$entity_type_id]['prompt_config'];
          }
        }
      }
    };
    $config
      ->set('enabled_entity_types', $enabled_entity_types)
      ->set('enabled_bundles', $enabled_bundles)
      ->set('bundle_prompts', $bundle_prompts)
      ->set('view_modes', $view_modes)
      ->save();

    parent::submitForm($form, $form_state);
  }

  /**
   * Redirects to the confirmation page after form submission.
   */
  public function redirectToConfirm(array &$form, FormStateInterface $form_state) {
    $form_state->setRedirect('ai_content_lifecycle.batch_confirm');
  }

  /**
   * Get available content entity types.
   *
   * @return \Drupal\Core\Entity\EntityTypeInterface[]
   *   Array of content entity types.
   */
  protected function getAvailableContentEntityTypes() {
    $entity_types = [];
    $definitions = $this->entityTypeManager->getDefinitions();

    foreach ($definitions as $entity_type_id => $entity_type) {
      // Only include content entities (skip config entities).
      if ($entity_type->entityClassImplements('\Drupal\Core\Entity\ContentEntityInterface')) {
        $entity_types[$entity_type_id] = $entity_type;
      }
    }

    // Sort by label.
    uasort($entity_types, function ($a, $b) {
      return strnatcasecmp($a->getLabel(), $b->getLabel());
    });

    return $entity_types;
  }

  /**
   * Get available view modes for an entity type.
   *
   * @param string $entity_type_id
   *   The entity type ID.
   *
   * @return array
   *   Array of view modes.
   */
  protected function getViewModeOptions($entity_type_id) {
    $view_modes = [];

    $view_mode_storage = $this->entityTypeManager->getStorage('entity_view_mode');
    $view_mode_ids = $view_mode_storage->getQuery()
      ->condition('targetEntityType', $entity_type_id)
      ->execute();

    if (!empty($view_mode_ids)) {
      $view_mode_entities = $view_mode_storage->loadMultiple($view_mode_ids);
      // Add default view mode
      $view_modes['default'] = $this->t('Default');

      foreach ($view_mode_entities as $view_mode) {
        $id = str_replace($entity_type_id . '.', '', $view_mode->id());
        $view_modes[$id] = $view_mode->label();
      }
    }

    if (empty($view_modes)) {
      $view_modes['default'] = $this->t('Default');
    }

    return $view_modes;
  }

}
