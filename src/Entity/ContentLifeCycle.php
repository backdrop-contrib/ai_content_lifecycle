<?php

declare(strict_types=1);

namespace Drupal\ai_content_lifecycle\Entity;

use Drupal\Core\Entity\EntityChangedTrait;
use Drupal\Core\Entity\EntityInterface;
use Drupal\Core\Entity\EntityStorageInterface;
use Drupal\Core\Entity\EntityTypeInterface;
use Drupal\Core\Entity\RevisionableContentEntityBase;
use Drupal\Core\Field\BaseFieldDefinition;
use Drupal\ai_content_lifecycle\ContentLifeCycleInterface;
use Drupal\user\EntityOwnerTrait;

/**
 * Defines the content life cycle entity class.
 *
 * @ContentEntityType(
 *   id = "content_life_cycle",
 *   label = @Translation("Content Life Cycle"),
 *   label_collection = @Translation("Content Life Cycle"),
 *   label_singular = @Translation("Content Life Cycle"),
 *   label_plural = @Translation("Content Life Cycles"),
 *   label_count = @PluralTranslation(
 *     singular = "@count content life cycles",
 *     plural = "@count content life cycles",
 *   ),
 *   handlers = {
 *     "list_builder" = "Drupal\ai_content_lifecycle\ContentLifeCycleListBuilder",
 *     "views_data" = "Drupal\views\EntityViewsData",
 *     "form" = {
 *       "add" = "Drupal\ai_content_lifecycle\Form\ContentLifeCycleForm",
 *       "edit" = "Drupal\ai_content_lifecycle\Form\ContentLifeCycleForm",
 *       "delete" = "Drupal\Core\Entity\ContentEntityDeleteForm",
 *       "delete-multiple-confirm" = "Drupal\Core\Entity\Form\DeleteMultipleForm",
 *       "revision-delete" = \Drupal\Core\Entity\Form\RevisionDeleteForm::class,
 *       "revision-revert" = \Drupal\Core\Entity\Form\RevisionRevertForm::class,
 *     },
 *     "route_provider" = {
 *       "html" = "Drupal\Core\Entity\Routing\AdminHtmlRouteProvider",
 *       "revision" = \Drupal\Core\Entity\Routing\RevisionHtmlRouteProvider::class,
 *     },
 *   },
 *   base_table = "content_life_cycle",
 *   data_table = "content_life_cycle_field_data",
 *   revision_table = "content_life_cycle_revision",
 *   revision_data_table = "content_life_cycle_field_revision",
 *   show_revision_ui = TRUE,
 *   translatable = TRUE,
 *   admin_permission = "administer content_life_cycle",
 *   entity_keys = {
 *     "id" = "id",
 *     "revision" = "revision_id",
 *     "langcode" = "langcode",
 *     "label" = "label",
 *     "uuid" = "uuid",
 *     "owner" = "uid",
 *   },
 *   revision_metadata_keys = {
 *     "revision_user" = "revision_uid",
 *     "revision_created" = "revision_timestamp",
 *     "revision_log_message" = "revision_log",
 *   },
 *   links = {
 *     "collection" = "/admin/content/content-life-cycle",
 *     "add-form" = "/admin/content/life-cycle-management/add",
 *     "canonical" = "/admin/content/life-cycle-management/{content_life_cycle}",
 *     "edit-form" = "/admin/content/life-cycle-management/{content_life_cycle}/edit",
 *     "delete-form" = "/admin/content/life-cycle-management/{content_life_cycle}/delete",
 *     "delete-multiple-form" = "/admin/content/content-life-cycle/delete-multiple",
 *     "revision" = "/admin/content/life-cycle-management/{content_life_cycle}/revision/{content_life_cycle_revision}/view",
 *     "revision-delete-form" = "/admin/content/life-cycle-management/{content_life_cycle}/revision/{content_life_cycle_revision}/delete",
 *     "revision-revert-form" = "/admin/content/life-cycle-management/{content_life_cycle}/revision/{content_life_cycle_revision}/revert",
 *     "version-history" = "/admin/content/life-cycle-management/{content_life_cycle}/revisions",
 *   },
 * )
 */
final class ContentLifeCycle extends RevisionableContentEntityBase implements ContentLifeCycleInterface {

  use EntityChangedTrait;
  use EntityOwnerTrait;

  /**
   * {@inheritdoc}
   */
  public function preSave(EntityStorageInterface $storage): void {
    parent::preSave($storage);
    if (!$this->getOwnerId()) {
      // If no owner has been set explicitly, make the anonymous user the owner.
      $this->setOwnerId(0);
    }
  }

  /**
   * {@inheritdoc}
   */
  public static function baseFieldDefinitions(EntityTypeInterface $entity_type): array {

    $fields = parent::baseFieldDefinitions($entity_type);

    $fields['label'] = BaseFieldDefinition::create('string')
      ->setRevisionable(TRUE)
      ->setTranslatable(TRUE)
      ->setLabel(t('Label'))
      ->setRequired(TRUE)
      ->setSetting('max_length', 255)
      ->setDisplayOptions('form', [
        'type' => 'string_textfield',
        'weight' => -5,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'hidden',
        'type' => 'string',
        'weight' => -5,
      ])
      ->setDisplayConfigurable('view', TRUE);

    // Add entity type field
    $fields['entity_type'] = BaseFieldDefinition::create('string')
      ->setLabel(t('Entity Type'))
      ->setDescription(t('The entity type of the content this lifecycle refers to.'))
      ->setRequired(TRUE)
      ->setTranslatable(FALSE)
      ->setRevisionable(TRUE)
      ->setSettings([
        'max_length' => 32,
        'text_processing' => 0,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'inline',
        'type' => 'string',
        'weight' => -4,
      ])
      ->setDisplayConfigurable('view', TRUE);

    // Add entity bundle field
    $fields['entity_bundle'] = BaseFieldDefinition::create('string')
      ->setLabel(t('Entity Bundle'))
      ->setDescription(t('The bundle of the content this lifecycle refers to.'))
      ->setRequired(TRUE)
      ->setTranslatable(FALSE)
      ->setRevisionable(TRUE)
      ->setSettings([
        'max_length' => 128,
        'text_processing' => 0,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'inline',
        'type' => 'string',
        'weight' => -3,
      ])
      ->setDisplayConfigurable('view', TRUE);

    // Add entity id field
    $fields['entity_id'] = BaseFieldDefinition::create('integer')
      ->setLabel(t('Entity ID'))
      ->setDescription(t('The ID of the content entity this lifecycle refers to.'))
      ->setRequired(TRUE)
      ->setTranslatable(FALSE)
      ->setRevisionable(TRUE)
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'inline',
        'type' => 'number_integer',
        'weight' => -2,
      ])
      ->setDisplayConfigurable('view', TRUE);

    $fields['uid'] = BaseFieldDefinition::create('entity_reference')
      ->setRevisionable(TRUE)
      ->setTranslatable(TRUE)
      ->setLabel(t('Author'))
      ->setSetting('target_type', 'user')
      ->setDefaultValueCallback(self::class . '::getDefaultEntityOwner')
      ->setDisplayOptions('form', [
        'type' => 'entity_reference_autocomplete',
        'settings' => [
          'match_operator' => 'CONTAINS',
          'size' => 60,
          'placeholder' => '',
        ],
        'weight' => 15,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'above',
        'type' => 'author',
        'weight' => 15,
      ])
      ->setDisplayConfigurable('view', TRUE);

    $fields['created'] = BaseFieldDefinition::create('created')
      ->setLabel(t('Authored on'))
      ->setTranslatable(TRUE)
      ->setDescription(t('The time that the content life cycle was created.'))
      ->setDisplayOptions('view', [
        'label' => 'above',
        'type' => 'timestamp',
        'weight' => 20,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('form', [
        'type' => 'datetime_timestamp',
        'weight' => 20,
      ])
      ->setDisplayConfigurable('view', TRUE);

    $fields['changed'] = BaseFieldDefinition::create('changed')
      ->setLabel(t('Changed'))
      ->setTranslatable(TRUE)
      ->setDescription(t('The time that the content life cycle was last edited.'));

    $fields['ai_prompt_results'] = BaseFieldDefinition::create('text_long')
      ->setLabel(t('AI evaluation results'))
      ->setDescription(t('The results of the AI prompt check.'))
      ->setRevisionable(TRUE)
      ->setTranslatable(TRUE)
      ->setDisplayOptions('form', [
        'type' => 'text_textarea',
        'weight' => 10,
        'rows' => 6,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayOptions('view', [
        'label' => 'above',
        'type' => 'text_default',
        'weight' => 10,
      ])
      ->setDisplayConfigurable('view', TRUE);

    $fields['review_status'] = BaseFieldDefinition::create('list_string')
      ->setLabel(t('Review Status'))
      ->setDescription(t('The current review status of this content lifecycle.'))
      ->setRequired(TRUE)
      ->setSetting('allowed_values', [
        'pending' => t('Pending'),
        'analyzed' => t('Analyzed'),
        'reviewed' => t('Reviewed'),
        'updated' => t('Updated'),
        'ignored' => t('Ignored'),
      ])
      ->setDefaultValue('pending')
      ->setDisplayOptions('view', [
        'label' => 'visible',
        'type' => 'list_default',
        'weight' => 0,
      ])
      ->setDisplayOptions('form', [
        'type' => 'options_select',
        'weight' => 0,
      ])
      ->setDisplayConfigurable('form', TRUE)
      ->setDisplayConfigurable('view', TRUE);

    return $fields;
  }

  /**
   * Sets the referenced entity.
   *
   * @param \Drupal\Core\Entity\EntityInterface $entity
   *   The entity to reference.
   *
   * @return $this
   */
  public function setReferencedEntity(EntityInterface $entity) {
    $this->set('entity_type', $entity->getEntityTypeId());

    // Not all entities have bundles
    $bundle = method_exists($entity, 'bundle') ? $entity->bundle() : $entity->getEntityTypeId();
    $this->set('entity_bundle', $bundle);

    $this->set('entity_id', $entity->id());
    return $this;
  }

  /**
   * Gets the referenced entity.
   *
   * @return \Drupal\Core\Entity\EntityInterface|null
   *   The referenced entity, or NULL if not found.
   */
  public function getReferencedEntity() {
    $entity_type = $this->get('entity_type')->value;
    $entity_id = $this->get('entity_id')->value;

    if (!$entity_type || !$entity_id) {
      return NULL;
    }

    try {
      return \Drupal::entityTypeManager()
        ->getStorage($entity_type)
        ->load($entity_id);
    }
    catch (\Exception $e) {
      \Drupal::logger('ai_content_lifecycle')->error(
        'Error loading referenced entity: @message', ['@message' => $e->getMessage()]
      );
      return NULL;
    }
  }

  public static function findForEntity(EntityInterface $entity) {
    // Not all entities have bundles
    $bundle = method_exists($entity, 'bundle') ? $entity->bundle() : $entity->getEntityTypeId();

    return \Drupal::entityTypeManager()
      ->getStorage('content_life_cycle')
      ->loadByProperties([
        'entity_type' => $entity->getEntityTypeId(),
        'entity_bundle' => $bundle,
        'entity_id' => $entity->id(),
      ]);
  }

}
