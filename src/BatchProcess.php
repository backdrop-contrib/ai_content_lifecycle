<?php

namespace Drupal\ai_content_lifecycle;

use Drupal\ai_content_lifecycle\Entity\ContentLifeCycle;
use Drupal\Core\StringTranslation\StringTranslationTrait;

/**
 * Batch processing for content lifecycle elements creation.
 */
class BatchProcess {

  use StringTranslationTrait;

  /**
   * Process a single bundle to create lifecycle elements.
   *
   * @param string $entity_type_id
   *   The entity type ID.
   * @param string $bundle_id
   *   The bundle ID.
   * @param array $context
   *   The batch context.
   */
  public static function processBundle($entity_type_id, $bundle_id, &$context) {
    if (!isset($context['results']['processed'])) {
      $context['results']['processed'] = 0;
      $context['results']['skipped'] = 0;
      $context['results']['updated'] = 0;
      $context['sandbox']['progress'] = 0;
      $context['sandbox']['current_id'] = 0;
      $context['sandbox']['max'] = 0;
    }

    // Initialize sandbox if needed
    if (empty($context['sandbox']['max'])) {
      // Get entity storage
      $entity_storage = \Drupal::entityTypeManager()->getStorage($entity_type_id);

      // Query to get all entity IDs of this bundle
      $query = $entity_storage->getQuery();
      if ($bundle_id && $entity_storage->getEntityType()->hasKey('bundle')) {
        $query->condition($entity_storage->getEntityType()->getKey('bundle'), $bundle_id);
      }

      $entity_ids = $query->accessCheck(FALSE)->execute();
      $total = count($entity_ids);

      $context['sandbox']['max'] = $total;
      $context['sandbox']['entity_ids'] = $entity_ids;

      if (empty($entity_ids)) {
        $context['finished'] = 1;
        return;
      }
    }

    // Process entities in batches of 10
    $limit = 2;
    $entity_ids = array_slice(
      $context['sandbox']['entity_ids'],
      $context['sandbox']['progress'],
      $limit
    );

    if (empty($entity_ids)) {
      $context['finished'] = 1;
      return;
    }

    $entity_storage = \Drupal::entityTypeManager()->getStorage($entity_type_id);
    $lifecycle_storage = \Drupal::entityTypeManager()->getStorage('content_life_cycle');
    $entities = $entity_storage->loadMultiple($entity_ids);
    /** @var \Drupal\ai_content_lifecycle\ContentAIAnalyzer $analyzer */
    $analyzer = \Drupal::service('ai_content_lifecycle.content_ai_analyzer');

    foreach ($entities as $entity) {
      try {
        // Check if lifecycle entity already exists
        $existing = ContentLifeCycle::findForEntity($entity);

        if (!empty($existing)) {
          $lifecycle = reset($existing);
          // Skip entities that are marked as reviewed or ignored
          if (in_array($lifecycle->get('review_status')->value, ['reviewed', 'ignored'])) {
            $context['results']['skipped']++;
            continue;
          }

          // Continue with analysis for other statuses.
          $result = $analyzer->analyzeContent($entity);
          // Only update lifecycle entity if content analysis returns results.
          if ($result !== NULL) {
            $lifecycle->setReferencedEntity($entity);
            $lifecycle->set('ai_prompt_results', $result);
            $lifecycle->set('review_status', 'analyzed');
            $lifecycle->save();
            $context['results']['updated']++;
          }
        }
        else {
          // Check if content needs analysis for new entities
          $result = $analyzer->analyzeContent($entity);
          // Only create lifecycle entity if content analysis returns results.
          if ($result !== NULL) {
            // Create new lifecycle entity.
            $lifecycle = $lifecycle_storage->create([
              'label' => $entity->label() ?? 'Lifecycle for ' . $entity_type_id . ':' . $entity->id(),
            ]);
            $lifecycle->setReferencedEntity($entity);
            $lifecycle->set('ai_prompt_results', $result);
            $lifecycle->set('review_status', 'analyzed');
            $lifecycle->save();
            $context['results']['processed']++;
          }
        }
      }
      catch (\Exception $e) {
        \Drupal::logger('ai_content_lifecycle')->error(
          'Error creating lifecycle for @type @id: @message',
          [
            '@type' => $entity_type_id,
            '@id' => $entity->id(),
            '@message' => $e->getMessage(),
          ]
        );
        $context['results']['skipped']++;
      }

      $context['sandbox']['progress']++;
    }

    // Update progress
    if ($context['sandbox']['max'] > 0) {
      $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
    }

    $context['message'] = t('Processing @entity_type: @bundle - @progress of @total', [
      '@entity_type' => $entity_type_id,
      '@bundle' => $bundle_id,
      '@progress' => $context['sandbox']['progress'],
      '@total' => $context['sandbox']['max'],
    ]);
  }

  /**
   * Batch finish callback.
   */
  public static function finished($success, $results, $operations) {
    if ($success) {
      $message = \Drupal::translation()->formatPlural(
        $results['processed'],
        'One content lifecycle element created successfully.',
        '@count content lifecycle elements created successfully.'
      );

      if (!empty($results['skipped'])) {
        $message .= ' ' . \Drupal::translation()->formatPlural(
            $results['skipped'],
            'One entity was skipped.',
            '@count entities were skipped.'
          );
      }
    }
    else {
      $message = t('An error occurred during batch processing.');
    }

    \Drupal::messenger()->addStatus($message);
  }

}
