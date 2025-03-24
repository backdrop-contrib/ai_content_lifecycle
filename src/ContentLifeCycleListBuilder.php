<?php

declare(strict_types=1);

namespace Drupal\ai_content_lifecycle;

use Drupal\Core\Entity\EntityInterface;
use Drupal\Core\Entity\EntityListBuilder;

/**
 * Provides a list controller for the content life cycle entity type.
 */
final class ContentLifeCycleListBuilder extends EntityListBuilder {

  /**
   * {@inheritdoc}
   */
  public function buildHeader(): array {
    $header['id'] = $this->t('ID');
    $header['label'] = $this->t('Label');
    $header['uid'] = $this->t('Author');
    $header['ai_prompt_results'] = $this->t('Results');
    $header['created'] = $this->t('Created');
    $header['changed'] = $this->t('Last checked');
    return $header + parent::buildHeader();
  }

  /**
   * {@inheritdoc}
   */
  public function buildRow(EntityInterface $entity): array {
    /** @var \Drupal\ai_content_lifecycle\ContentLifeCycleInterface $entity */
    $row['id'] = $entity->id();
    $row['label'] = $entity->toLink();
    $username_options = [
      'label' => 'hidden',
      'settings' => ['link' => $entity->get('uid')->entity->isAuthenticated()],
    ];
    $row['uid']['data'] = $entity->get('uid')->view($username_options);
    $row['ai_prompt_results']['data'] = $entity->get('ai_prompt_results')->view(['label' => 'hidden']);
    $row['created']['data'] = $entity->get('created')->view(['label' => 'hidden']);
    $row['changed']['data'] = $entity->get('changed')->view(['label' => 'hidden']);
    return $row + parent::buildRow($entity);
  }

}
