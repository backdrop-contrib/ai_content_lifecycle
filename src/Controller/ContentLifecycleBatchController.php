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
   * Batch creates content lifecycle elements for all enabled entities.
   */
  public function batchCreate() {
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
