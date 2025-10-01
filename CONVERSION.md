# Drupal 11 to Backdrop CMS Conversion Notes

## OpenAI Content Lifecycle Module

This document describes the conversion of the Drupal 11 `ai_content_lifecycle` module to Backdrop CMS as `openai_content_lifecycle`.

## Major Changes

### 1. Module Rename
- **From:** `ai_content_lifecycle`
- **To:** `openai_content_lifecycle`
- **Reason:** To align with Backdrop's OpenAI module namespace instead of the Drupal AI module

### 2. Dependency Changes
- **Removed:** `ai:ai` (Drupal AI module)
- **Added:** `openai` (Backdrop OpenAI module)
- **Added:** `entity_plus` (Required for custom entities in Backdrop)
- **Added:** `entity_ui` (For entity UI support)

### 3. Architecture Changes

#### From Drupal 11 Structure:
- Used Drupal 8/9/10/11 annotations (`@ContentEntityType`)
- PSR-4 autoloading with namespaces (`Drupal\ai_content_lifecycle\...`)
- Symfony-based routing (`*.routing.yml`)
- Configuration schemas (`*.schema.yml`)
- Services container (`*.services.yml`)
- Form classes extending `ConfigFormBase`

#### To Backdrop Structure:
- Uses `hook_entity_info()` for entity definitions
- Simple autoload via `hook_autoload_info()`
- Menu system via `hook_menu()`
- Configuration via `config()` API
- Form functions instead of classes
- Entity class extending `Entity` from Entity Plus

### 4. Entity System

**Drupal 11:** Used `RevisionableContentEntityBase` with field definitions
```php
@ContentEntityType(
  id = "content_life_cycle",
  ...
)
class ContentLifeCycle extends RevisionableContentEntityBase
```

**Backdrop:** Uses Entity Plus with `hook_entity_info()` and database schema
```php
function openai_content_lifecycle_entity_info() {
  $info['content_lifecycle'] = array(
    'controller class' => 'EntityPlusController',
    'entity class' => 'ContentLifecycle',
    ...
  );
}
```

### 5. AI Integration

**Drupal 11:** Used the `ai` module's provider system
```php
use Drupal\ai\AiProviderPluginManager;
use Drupal\ai\OperationType\Chat\ChatInput;
use Drupal\ai\OperationType\Chat\ChatMessage;

$provider = $this->aiProvider->createInstance($sets['provider_id']);
$response = $provider->chat($messages, $sets['model_id']);
```

**Backdrop:** Uses the `openai` module's OpenAIApi class directly
```php
$this->openaiApi = new OpenAIApi($api_key);
$messages = array(
  array('role' => 'system', 'content' => '...'),
  array('role' => 'user', 'content' => '...'),
);
$response = $this->openaiApi->chat($model, $messages, $temperature);
```

### 6. Configuration

**Drupal 11:** YAML-based configuration files
- `config/install/*.yml`
- `config/schema/*.yml`

**Backdrop:** JSON-based configuration
- `config/*.settings.json`
- Managed via `config()` API

### 7. Routing and Forms

**Drupal 11:** Routing file + Form classes
```yaml
# *.routing.yml
ai_content_lifecycle.settings:
  path: '/admin/config/ai/lifecycle'
  defaults:
    _form: '\Drupal\ai_content_lifecycle\Form\ContentLifecycleSettingsForm'
```

**Backdrop:** Hook_menu + Form functions
```php
function openai_content_lifecycle_menu() {
  $items['admin/config/content/openai-content-lifecycle'] = array(
    'page callback' => 'backdrop_get_form',
    'page arguments' => array('openai_content_lifecycle_settings_form'),
    ...
  );
}
```

### 8. Database Schema

**Drupal 11:** Generated automatically from entity field definitions

**Backdrop:** Explicit `hook_schema()` implementation
```php
function openai_content_lifecycle_schema() {
  $schema['content_lifecycle'] = array(
    'fields' => array(...),
    'primary key' => array('id'),
    'indexes' => array(...),
  );
}
```

## Files Converted

### Core Module Files
- `openai_content_lifecycle.info` - Module info file (converted from .yml)
- `openai_content_lifecycle.module` - Hook implementations
- `openai_content_lifecycle.install` - Installation and schema

### Include Files
- `includes/OpenAIContentLifecycleAnalyzer.inc` - Main AI analysis logic
- `includes/ContentLifecycle.inc` - Entity class
- `includes/openai_content_lifecycle.admin.inc` - Admin forms

### Configuration
- `config/openai_content_lifecycle.settings.json` - Default configuration

## Files Removed (Drupal-specific)
- `src/` directory (PSR-4 namespaced classes)
- `*.routing.yml` (Symfony routing)
- `*.services.yml` (Dependency injection)
- `*.permissions.yml` (Permissions defined in hook now)
- `*.links.*.yml` (Menu links)
- `*.libraries.yml` (Asset libraries)
- `config/install/*.yml` (Configuration schemas)
- `config/schema/*.yml` (Configuration schemas)
- `templates/` (Would need conversion to Backdrop template system if needed)

## Testing Checklist

- [x] Module enables without errors
- [x] Database schema created successfully
- [x] Entity class loads properly
- [ ] Configuration form accessible
- [ ] Can save configuration
- [ ] Can analyze content with OpenAI
- [ ] Lifecycle entities created and stored
- [ ] Batch processing works
- [ ] Views integration works
- [ ] Permissions work correctly

## Known Limitations

1. **No Revisions:** The Backdrop version doesn't implement revisions (unlike Drupal version)
2. **Simplified UI:** The admin UI is simplified compared to Drupal version
3. **No Views Export:** The optional Views export wasn't converted
4. **No Field UI Integration:** Simplified field management

## Usage

1. Install dependencies: `openai`, `entity_plus`, `entity_ui`, `key`
2. Enable module: `ddev bee en openai_content_lifecycle -y`
3. Configure at: `admin/config/content/openai-content-lifecycle`
4. Set OpenAI model and conditions
5. Select content types to analyze
6. Run batch analysis or analyze individual content

## API Differences

### Creating a Lifecycle Entity

**Drupal 11:**
```php
$lifecycle = $this->entityTypeManager->getStorage('content_life_cycle')->create([
  'label' => $entity->label(),
]);
$lifecycle->setReferencedEntity($entity);
$lifecycle->save();
```

**Backdrop:**
```php
$lifecycle = entity_create('content_lifecycle', array());
$lifecycle->setReferencedEntity($entity);
$lifecycle->save();
```

### Finding Lifecycle for Entity

**Drupal 11:**
```php
$existing = ContentLifeCycle::findForEntity($entity);
```

**Backdrop:**
```php
$existing = ContentLifecycle::findForEntity($entity);
```

## Future Enhancements

- Add Views integration for lifecycle entities
- Create a field formatter for displaying AI results on nodes
- Add batch API for background processing
- Implement a simple admin UI for reviewing flagged content
- Add token support for custom prompts
- Support for more entity types beyond nodes

## Credits

- Original Drupal module: ayalon, wouters_f
- Backdrop conversion: Automated conversion with manual refinements
- Uses Entity Plus module for custom entity support
- Integrates with Backdrop's OpenAI module
