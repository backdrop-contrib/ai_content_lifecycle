<?php

/**
 * @file
 * Entity class for Content Lifecycle.
 */

/**
 * The class used for content lifecycle entities.
 */
class ContentLifecycle extends Entity {

  /**
   * The content lifecycle ID.
   *
   * @var int
   */
  public $id;

  /**
   * The label of the content lifecycle.
   *
   * @var string
   */
  public $label;

  /**
   * The entity type being referenced.
   *
   * @var string
   */
  public $entity_type;

  /**
   * The bundle of the entity being referenced.
   *
   * @var string
   */
  public $entity_bundle;

  /**
   * The ID of the entity being referenced.
   *
   * @var int
   */
  public $entity_id;

  /**
   * The AI analysis results.
   *
   * @var string
   */
  public $ai_results;

  /**
   * The review status.
   *
   * @var string
   */
  public $review_status;

  /**
   * The user ID of the owner.
   *
   * @var int
   */
  public $uid;

  /**
   * The Unix timestamp when the lifecycle was created.
   *
   * @var int
   */
  public $created;

  /**
   * The Unix timestamp when the lifecycle was most recently changed.
   *
   * @var int
   */
  public $changed;

  /**
   * Constructor for ContentLifecycle entity.
   */
  public function __construct(array $values = array()) {
    parent::__construct($values, 'content_lifecycle');

    // Set defaults.
    if (!isset($this->review_status)) {
      $this->review_status = 'pending';
    }
    if (!isset($this->created)) {
      $this->created = REQUEST_TIME;
    }
    if (!isset($this->changed)) {
      $this->changed = REQUEST_TIME;
    }
    if (!isset($this->uid)) {
      global $user;
      $this->uid = $user->uid;
    }
  }

  /**
   * Sets the referenced entity for this lifecycle.
   *
   * @param object $entity
   *   The entity to reference.
   *
   * @return ContentLifecycle
   *   The current lifecycle entity for method chaining.
   */
  public function setReferencedEntity($entity) {
    $entity_type_name = $entity->entityType();
    $this->entity_type = $entity_type_name;
    $this->entity_bundle = isset($entity->type) ? $entity->type : $entity_type_name;

    // Get entity ID using entity_extract_ids().
    list($id, $vid, $bundle) = entity_extract_ids($entity_type_name, $entity);
    $this->entity_id = $id;

    if (empty($this->label)) {
      if (isset($entity->title)) {
        $this->label = $entity->title;
      }
      else {
        $this->label = $this->entity_type . ':' . $this->entity_id;
      }
    }

    return $this;
  }

  /**
   * Gets the referenced entity.
   *
   * @return object|NULL
   *   The referenced entity or NULL if not found.
   */
  public function getReferencedEntity() {
    if (empty($this->entity_type) || empty($this->entity_id)) {
      return NULL;
    }

    $entities = entity_load($this->entity_type, array($this->entity_id));
    return !empty($entities[$this->entity_id]) ? $entities[$this->entity_id] : NULL;
  }

  /**
   * Find lifecycle entities for a given entity.
   *
   * @param object $entity
   *   The entity to find lifecycles for.
   *
   * @return array
   *   Array of ContentLifecycle entities.
   */
  public static function findForEntity($entity) {
    $entity_type = $entity->entityType();
    $bundle = isset($entity->type) ? $entity->type : $entity_type;

    // Get entity ID using entity_extract_ids().
    list($entity_id, $vid, $bundle_extracted) = entity_extract_ids($entity_type, $entity);

    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'content_lifecycle')
      ->propertyCondition('entity_type', $entity_type)
      ->propertyCondition('entity_bundle', $bundle)
      ->propertyCondition('entity_id', $entity_id);

    $result = $query->execute();

    if (isset($result['content_lifecycle'])) {
      return entity_load('content_lifecycle', array_keys($result['content_lifecycle']));
    }

    return array();
  }

  /**
   * Overrides Entity::save().
   */
  public function save() {
    $this->changed = REQUEST_TIME;
    return parent::save();
  }

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->id) ? $this->id : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'content_lifecycle';
  }

  /**
   * Implements EntityInterface::uri().
   */
  public function uri() {
    return $this->defaultUri();
  }

  /**
   * Overrides Entity::defaultLabel().
   */
  protected function defaultLabel() {
    return $this->label;
  }

  /**
   * Overrides Entity::defaultUri().
   */
  protected function defaultUri() {
    return array('path' => 'content-lifecycle/' . $this->id);
  }
}
