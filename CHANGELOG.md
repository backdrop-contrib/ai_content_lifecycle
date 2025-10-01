# Changelog - OpenAI Content Lifecycle for Backdrop CMS

## Version 1.x-1.0.0-dev

### Initial Conversion from Drupal 11

**Date:** October 1, 2025

#### Major Changes
- Converted Drupal 11 `ai_content_lifecycle` module to Backdrop CMS
- Renamed module to `openai_content_lifecycle` to align with Backdrop's OpenAI module
- Replaced Drupal AI module integration with Backdrop OpenAI module
- Implemented custom entity using Entity Plus module
- Created database schema for `content_lifecycle` entity

#### Form Improvements (Matching Drupal UX)
- **Reordered form elements** to put most important field first:
  1. "Mark my content when" - Main prompt field (now first)
  2. "Advanced LLM settings" - Collapsed by default
  3. "What to check" - Content type selection
  
- **Updated field labels** to match Drupal version:
  - Changed "Default Analysis Conditions" → **"Mark my content when"**
  - Changed "Content Types to Analyze" → **"What to check"**
  - Changed "Specific conditions for [type]" → **"Mark [type] when"**
  - Changed "System Prompt Template" → **"Pre prompt"**
  - Changed "OpenAI Model" → **"LLM to use for content evaluation"**
  - Changed "AI Model Settings" → **"Advanced LLM settings"**

- **Added placeholder examples** showing sample conditions
- **Increased textarea rows** for prompts (6 rows for type-specific, 25 for pre-prompt)
- **Collapsed AI settings by default** (advanced settings)
- **Improved descriptions** with HTML formatting for better readability

#### Bug Fixes
- **Fixed temperature validation** 
  - Now properly accepts numeric values between 0 and 2
  - Added `element_validate_number` to form element
  - Converts temperature to float on save
  - Stores as numeric value in config (not string)

#### Technical Implementation

**Entity System:**
- Custom `content_lifecycle` entity with fields:
  - `id` - Primary key
  - `label` - Entity label
  - `entity_type`, `entity_bundle`, `entity_id` - Reference to content
  - `ai_results` - AI analysis text
  - `review_status` - Workflow status
  - `uid`, `created`, `changed` - Tracking fields

**Dependencies:**
- `openai` - For AI API calls
- `entity_plus` - For custom entity support
- `entity_ui` - For entity UI
- `key` - For secure API key storage (via openai module)

**Files Created:**
- `openai_content_lifecycle.info` - Module definition
- `openai_content_lifecycle.module` - Hooks and entity info
- `openai_content_lifecycle.install` - Schema and installation
- `includes/ContentLifecycle.inc` - Entity class
- `includes/OpenAIContentLifecycleAnalyzer.inc` - AI analysis engine
- `includes/openai_content_lifecycle.admin.inc` - Admin forms
- `config/openai_content_lifecycle.settings.json` - Default config

**Documentation:**
- `README.md` - User guide
- `CONVERSION.md` - Technical conversion notes
- `TESTING.md` - Testing procedures
- `CHANGELOG.md` - This file

#### Known Limitations
1. No revision support (unlike Drupal version)
2. Only supports nodes currently (Drupal version supports all entity types)
3. No Views export included (would need conversion)
4. Simplified admin UI compared to Drupal

#### Configuration
Default configuration includes:
- Model: gpt-4o-mini (cost-effective)
- Temperature: 0.7 (balanced)
- Pre-prompt: Standard XTRUE/XFALSE detection system
- Default conditions: Inconsistencies and outdated content

#### Permissions
- `administer openai content lifecycle` - Full admin access
- `view content lifecycle data` - View analysis results
- `edit content lifecycle data` - Edit/update lifecycle entities

#### Database Schema
Table: `content_lifecycle`
- Indexes on: entity_type, entity_bundle, entity_id, review_status, uid, created, changed
- Composite index on: entity_type + entity_bundle + entity_id

#### API Usage
Two-step analysis process:
1. Initial detection (XTRUE/XFALSE) - Minimal tokens
2. Detailed explanation - Only if flagged

This approach minimizes API costs while providing useful feedback.

#### Testing Status
- ✅ Module enables successfully
- ✅ Database schema created
- ✅ Entity class loads properly
- ✅ Temperature validation working
- ✅ Form displays correctly
- ✅ Configuration saves properly
- ⬜ Content analysis tested
- ⬜ Batch processing tested
- ⬜ Views integration tested

#### Upgrade Path
N/A - Initial release for Backdrop CMS

#### Credits
- Original Drupal module: ayalon, wouters_f
- Backdrop conversion: AI-assisted with human oversight
- Entity Plus integration: Based on Backdrop community patterns
- OpenAI integration: Uses Backdrop's OpenAI module

---

## Future Enhancements

### Planned Features
- [ ] Support for additional entity types (not just nodes)
- [ ] Views default configuration
- [ ] Admin UI for reviewing flagged content
- [ ] Bulk operations (mark as reviewed, ignore, etc.)
- [ ] Email notifications for content editors
- [ ] Dashboard widget showing content needing review
- [ ] Token support in prompts
- [ ] Scheduled batch processing via cron
- [ ] Export/import of prompt templates

### Under Consideration
- [ ] Integration with workflow modules
- [ ] Content quality scoring
- [ ] Historical analysis tracking
- [ ] Multi-language support improvements
- [ ] Field-level analysis (not just full content)
- [ ] Comparison with previous versions
- [ ] AI model selection per content type

### Community Requests
Please submit feature requests and bug reports to the project issue queue.

---

## Compatibility

### Backdrop CMS
- Tested on: 1.32.0
- Minimum required: 1.x

### Dependencies
- openai: 1.x
- entity_plus: 1.x
- entity_ui: 1.x  
- key: (via openai module)

### PHP
- Minimum: 8.2
- Tested on: 8.3

### Browsers
- Modern browsers with JavaScript enabled
- Admin UI tested in: Chrome, Firefox, Safari, Edge

---

## Support

For support, please:
1. Check the README.md for usage instructions
2. Review TESTING.md for troubleshooting
3. Check watchdog logs for errors
4. Submit issues with detailed information

## License

GPL-2.0-or-later (same as Backdrop CMS)
