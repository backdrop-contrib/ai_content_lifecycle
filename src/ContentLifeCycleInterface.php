<?php

declare(strict_types=1);

namespace Drupal\ai_content_lifecycle;

use Drupal\Core\Entity\ContentEntityInterface;
use Drupal\Core\Entity\EntityChangedInterface;
use Drupal\user\EntityOwnerInterface;

/**
 * Provides an interface defining a content life cycle entity type.
 */
interface ContentLifeCycleInterface extends ContentEntityInterface, EntityOwnerInterface, EntityChangedInterface {

}
