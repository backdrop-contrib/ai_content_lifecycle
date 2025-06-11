<?php

namespace Drupal\ai_content_lifecycle\Controller;

use Drupal\Core\Controller\ControllerBase;
use Drupal\Core\Url;
use Symfony\Component\DependencyInjection\ContainerInterface;
use Drupal\Core\Config\ConfigFactoryInterface;
use Drupal\Core\Entity\EntityTypeManagerInterface;
use Drupal\Core\Entity\EntityTypeBundleInfoInterface;
use Drupal\Core\Batch\BatchBuilder;
use Symfony\Component\HttpFoundation\RedirectResponse;

/**
 * Controller for the AI Content Lifecycle batch operations.
 */
class ContentLifecycleBatchController extends ControllerBase {

  /**
   * The config factory.
   *
   * @var \Drupal\Core\Config\ConfigFactoryInterface
   */
  protected $configFactory;

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
   * Constructs a ContentLifecycleBatchController object.
   *
   * @param \Drupal\Core\Config\ConfigFactoryInterface $config_factory
   *   The config factory.
   * @param \Drupal\Core\Entity\EntityTypeManagerInterface $entity_type_manager
   *   The entity type manager.
   * @param \Drupal\Core\Entity\EntityTypeBundleInfoInterface $entity_type_bundle_info
   *   The entity type bundle info.
   */
  public function __construct(
    ConfigFactoryInterface $config_factory,
    EntityTypeManagerInterface $entity_type_manager,
    EntityTypeBundleInfoInterface $entity_type_bundle_info,
  ) {
    $this->configFactory = $config_factory;
    $this->entityTypeManager = $entity_type_manager;
    $this->entityTypeBundleInfo = $entity_type_bundle_info;
  }

  /**
   * {@inheritdoc}
   */
  public static function create(ContainerInterface $container) {
    return new static(
      $container->get('config.factory'),
      $container->get('entity_type.manager'),
      $container->get('entity_type.bundle.info')
    );
  }

  /**
   * Shows a confirmation screen before starting the batch process.
   */
  public function batchConfirmation() {
    // Get fresh config directly from the database to ensure we have the latest values
    $this->configFactory->reset('ai_content_lifecycle.settings');
    $config = $this->configFactory->get('ai_content_lifecycle.settings');
    $enabled_entity_types = $config->get('enabled_entity_types') ?: [];
    $enabled_bundles = $config->get('enabled_bundles') ?: [];
    $bundle_prompts = $config->get('bundle_prompts') ?: [];
    $default_model = $config->get('default_model');
    $default_prompt = $config->get('default_prompt');

    if (empty($enabled_entity_types)) {
      $this->messenger()->addWarning($this->t('No entity types are enabled for AI Content Lifecycle.'));
      return new RedirectResponse(Url::fromRoute('ai_content_lifecycle.settings')->toString());
    }

    // Build confirmation data
    $confirmation = [
      '#theme' => 'ai_content_lifecycle_confirmation',
      '#confirmation' => [
        'message' => $this->t('You are about to analyze content using AI. This will create or update content lifecycle records for all selected content types.'),
        'model' => [
          'info' => $default_model ? $this->t('Using model: @model', ['@model' => $default_model]) : $this->t('Using default AI model from AI module'),
        ],
        'prompt' => [
          'info' => nl2br($default_prompt),
        ],
        'content_types' => [
          'list' => $this->buildContentTypesList($enabled_entity_types, $enabled_bundles),
        ],
        'actions' => [
          'process' => [
            '#type' => 'link',
            '#title' => $this->t('Analyze'),
            '#url' => Url::fromRoute('ai_content_lifecycle.batch_create'),
            '#attributes' => [
              'class' => ['button', 'button--primary'],
            ],
          ],
          'cancel' => [
            '#type' => 'link',
            '#title' => $this->t('Cancel'),
            '#url' => Url::fromRoute('ai_content_lifecycle.settings'),
            '#attributes' => [
              'class' => ['button'],
            ],
          ],
        ],
      ],
    ];

    return $confirmation;
  }

  /**
   * Builds the HTML list of content types to be analyzed.
   *
   * @param array $enabled_entity_types
   *   Array of enabled entity types.
   * @param array $enabled_bundles
   *   Array of enabled bundles.
   *
   * @return array
   *   Renderable array for the content types list.
   */
  protected function buildContentTypesList(array $enabled_entity_types, array $enabled_bundles) {
    $content_types = [];
    $bundle_prompts = $this->configFactory->get('ai_content_lifecycle.settings')->get('bundle_prompts') ?: [];
    $default_prompt = $this->configFactory->get('ai_content_lifecycle.settings')->get('default_prompt');

    foreach ($enabled_entity_types as $entity_type_id => $enabled) {
      if (!$enabled) {
        continue;
      }

      $entity_type = $this->entityTypeManager->getDefinition($entity_type_id);
      $item = [
        '#type' => 'container',
        'label' => [
          '#markup' => $entity_type->getLabel(),
        ],
      ];

      // Add entity-type specific prompt if exists
      if (isset($bundle_prompts[$entity_type_id]['default'])) {
        $entity_prompt = is_array($bundle_prompts[$entity_type_id]['default'])
          ? $bundle_prompts[$entity_type_id]['default']['value']
          : $bundle_prompts[$entity_type_id]['default'];

        if (!empty($entity_prompt)) {
          $item['prompt'] = [
            '#type' => 'container',
            '#attributes' => ['class' => ['entity-prompt']],
            'title' => [
              '#markup' => '<strong>' . $this->t('Conditions:') . '</strong>',
            ],
            'content' => [
              '#markup' => nl2br($entity_prompt),
              '#prefix' => '<div class="prompt-preview entity-type-prompt">',
              '#suffix' => '</div>',
            ],
          ];
        }
      }

      if (isset($enabled_bundles[$entity_type_id]) && !empty($enabled_bundles[$entity_type_id])) {
        $bundles = $this->entityTypeBundleInfo->getBundleInfo($entity_type_id);
        $bundle_list = [];

        foreach ($enabled_bundles[$entity_type_id] as $bundle_id => $bundle_enabled) {
          if ($bundle_enabled) {
            $bundle_item = [
              '#type' => 'container',
              'label' => [
                '#markup' => $bundles[$bundle_id]['label'],
              ],
            ];

            // Add bundle-specific prompt if exists
            if (isset($bundle_prompts[$entity_type_id][$bundle_id])) {
              $bundle_prompt = is_array($bundle_prompts[$entity_type_id][$bundle_id])
                ? $bundle_prompts[$entity_type_id][$bundle_id]['value']
                : $bundle_prompts[$entity_type_id][$bundle_id];

              if (!empty($bundle_prompt)) {
                $bundle_item['prompt'] = [
                  '#type' => 'container',
                  '#attributes' => ['class' => ['bundle-prompt']],
                  'title' => [
                    '#markup' => '<strong>' . $this->t('Conditions:') . '</strong>',
                  ],
                  'content' => [
                    '#markup' => nl2br($bundle_prompt),
                    '#prefix' => '<div class="prompt-preview bundle-prompt">',
                    '#suffix' => '</div>',
                  ],
                ];
              }
            }

            $bundle_list[] = [
              'data' => $bundle_item,
            ];
          }
        }

        if (!empty($bundle_list)) {
          $item['bundles'] = [
            '#theme' => 'item_list',
            '#items' => $bundle_list,
            '#prefix' => '<div class="bundle-list">',
            '#suffix' => '</div>',
          ];
        }
      }

      $content_types[] = [
        'data' => $item,
      ];
    }

    return [
      '#theme' => 'item_list',
      '#items' => $content_types,
    ];
  }

  /**
   * Batch creates content lifecycle elements for all enabled entities.
   */
  public function batchCreate() {
    // Get fresh config directly from the database to ensure we have the latest values
    $this->configFactory->reset('ai_content_lifecycle.settings');
    $config = $this->configFactory->get('ai_content_lifecycle.settings');
    $enabled_entity_types = $config->get('enabled_entity_types') ?: [];
    $enabled_bundles = $config->get('enabled_bundles') ?: [];

    if (empty($enabled_entity_types)) {
      $this->messenger()->addWarning($this->t('No entity types are enabled for AI Content Lifecycle.'));
      return new RedirectResponse(Url::fromRoute('ai_content_lifecycle.settings')->toString());
    }

    $operations = [];

    // Create batch operations for each enabled entity type and bundle
    foreach ($enabled_entity_types as $entity_type_id => $enabled) {
      if (!$enabled) {
        continue;
      }

      if (isset($enabled_bundles[$entity_type_id]) && !empty($enabled_bundles[$entity_type_id])) {
        foreach ($enabled_bundles[$entity_type_id] as $bundle_id => $bundle_enabled) {
          if ($bundle_enabled) {
            $operations[] = [
              '\Drupal\ai_content_lifecycle\BatchProcess::processBundle',
              [$entity_type_id, $bundle_id],
            ];
          }
        }
      }
      else {
        // For entity types without bundles
        $operations[] = [
          '\Drupal\ai_content_lifecycle\BatchProcess::processBundle',
          [$entity_type_id, NULL],
        ];
      }
    }

    $batch_builder = new BatchBuilder();
    $batch_builder
      ->setTitle($this->t('Analyzing content'))
      ->setFinishCallback('\Drupal\ai_content_lifecycle\BatchProcess::finished');

    // Add operations correctly
    foreach ($operations as $operation) {
      $batch_builder->addOperation($operation[0], $operation[1]);
    }

    batch_set($batch_builder->toArray());
    return batch_process(Url::fromRoute('ai_content_lifecycle.settings'));
  }

}
