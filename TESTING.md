# Testing Guide for OpenAI Content Lifecycle

## Module Status

✅ **Module is installed and enabled**
- Database table `content_lifecycle` created successfully
- All PHP files pass syntax validation
- No installation errors

## Quick Test

### 1. Access Configuration Page
```
URL: /admin/config/content/openai-content-lifecycle
Permission Required: "administer openai content lifecycle"
```

### 2. Configure the Module

#### AI Settings:
- **OpenAI Model**: Select from available GPT models (default: gpt-4o-mini)
- **Temperature**: Set between 0 and 2 (default: 0.7)
  - Lower values (0.1-0.5) = more focused/deterministic
  - Higher values (0.8-1.5) = more creative/random
- **System Prompt Template**: Customize the base prompt (uses tokens)

#### Default Analysis Conditions:
Define what should trigger a content review, for example:
```
- mentions outdated tax rates
- references discontinued products
- contains broken links or outdated statistics
- uses deprecated terminology
```

#### Content Type Selection:
- Check which node types to analyze (e.g., Article, Page)
- Optionally set specific conditions for each content type

### 3. Test Batch Analysis

1. Save your configuration
2. Click "Analyze Existing Content"
3. Confirm you understand API costs
4. Monitor the batch process
5. Check results in database:

```sql
SELECT * FROM content_lifecycle ORDER BY created DESC LIMIT 10;
```

## Manual Testing via Bee

### Test Entity Creation
```bash
# Create a test lifecycle entity via PHP
ddev exec 'php -r "
define(\"BACKDROP_ROOT\", \"/var/www/html\");
require_once BACKDROP_ROOT . \"/core/includes/bootstrap.inc\";
backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);

// Load a node to test
\$node = node_load(1);
if (\$node) {
  \$lifecycle = entity_create(\"content_lifecycle\", array());
  \$lifecycle->setReferencedEntity(\$node);
  \$lifecycle->ai_results = \"Test analysis result\";
  \$lifecycle->review_status = \"analyzed\";
  \$lifecycle->save();
  echo \"Created lifecycle entity ID: \" . \$lifecycle->id . \"\\n\";
}
"'
```

### Test Analyzer
```bash
# Test the OpenAI analyzer
ddev exec 'php -r "
define(\"BACKDROP_ROOT\", \"/var/www/html\");
require_once BACKDROP_ROOT . \"/core/includes/bootstrap.inc\";
backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);

\$node = node_load(1);
if (\$node) {
  \$analyzer = new OpenAIContentLifecycleAnalyzer();
  \$content = \$analyzer->analyzeContent(\$node);
  echo \"Analysis result: \" . (\$content ? \$content : \"No issues found\") . \"\\n\";
}
"'
```

## Expected Behavior

### When Content Needs Review (AI returns XTRUE):
1. Lifecycle entity is created/updated
2. `ai_results` field contains explanation from AI
3. `review_status` set to "analyzed"
4. Content flagged for editor review

### When Content is OK (AI returns XFALSE):
1. No lifecycle entity created
2. Node skipped in batch processing
3. No flag set

## Troubleshooting

### Check Watchdog Logs
```bash
ddev bee watchdog-show --tail --count=20
```

### Verify OpenAI API Key
```bash
ddev bee config-get openai.settings api_key
```

### Check Entity Info
```bash
ddev exec 'php -r "
define(\"BACKDROP_ROOT\", \"/var/www/html\");
require_once BACKDROP_ROOT . \"/core/includes/bootstrap.inc\";
backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);

\$info = entity_get_info(\"content_lifecycle\");
print_r(\$info);
"'
```

### Check Configuration
```bash
ddev bee config-get openai_content_lifecycle.settings
```

## Common Issues and Fixes

### Issue: "Temperature must be a number between 0 and 2"
**Fixed**: Temperature validation now properly handles numeric strings and converts to float

### Issue: Table doesn't exist
**Solution**: 
```bash
ddev bee dis openai_content_lifecycle -y
ddev bee pm-uninstall openai_content_lifecycle -y
ddev bee en openai_content_lifecycle -y
```

### Issue: OpenAI API errors
**Check**:
1. API key is configured in OpenAI module
2. Key module is enabled
3. API key has credits/permissions
4. Check OpenAI API status

### Issue: "Class not found"
**Solution**:
```bash
ddev bee cc all
```

## Performance Notes

- Each content analysis = 2 OpenAI API calls (detection + explanation)
- Batch processing includes 0.1s delay between items to avoid rate limiting
- Recommended: Start with small content type selection for testing
- Monitor OpenAI API usage/costs

## Integration Points

### Flag Module (Optional)
If Flag module is installed and has a "needs_review" flag:
- Automatically sets flag on analyzed content
- Provides visual indicator for editors

### Views Integration
Create custom Views to display:
- All flagged content
- Content by review status
- Recent AI analyses
- Content by entity type/bundle

Example View:
```
Base: Content Lifecycle
Relationships: 
  - Content Lifecycle -> Node (entity_id)
Fields:
  - Node title
  - AI Results
  - Review Status
  - Created date
Filters:
  - Review Status = "analyzed"
Sort:
  - Created date (DESC)
```

## Next Steps

1. ✅ Module conversion complete
2. ✅ Temperature validation fixed
3. ✅ Entity system working
4. ✅ Database schema installed
5. ⬜ Test with actual content
6. ⬜ Configure OpenAI prompts for your use case
7. ⬜ Create Views for content review workflow
8. ⬜ Train editors on using the system
9. ⬜ Monitor API costs and adjust batch sizes

## Support

For issues or questions:
1. Check watchdog logs first
2. Verify OpenAI module configuration
3. Review CONVERSION.md for architectural details
4. Check README.md for usage instructions
