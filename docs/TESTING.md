# Testing Guide — AI Content Lifecycle

## Prerequisites

- AI module configured with at least one text provider and API key.
- `ai_content_lifecycle` module enabled.
- At least one content type enabled in module settings.

## 1. Configuration Page

**URL:** `/admin/config/ai/ai-content-lifecycle`  
**Permission:** `administer ai content lifecycle`

Verify:
- AI model dropdown shows available text models.
- Temperature field accepts values between 0 and 2 (rejects text and out-of-range numbers).
- "Mark my content when" textarea saves and reloads correctly.
- Enabling a content type appears in per-type prompt section.
- Settings save without errors.

## 2. Batch Analysis

**URL:** `/admin/config/ai/ai-content-lifecycle/batch`

1. Enable at least one content type and save settings.
2. Navigate to the batch page and submit the form.
3. Confirm the batch completes without fatal errors.
4. After completion, check that `content_lifecycle` rows exist:

```sql
SELECT review_status, COUNT(*) FROM content_lifecycle GROUP BY review_status;
```

Expected: rows with `ok` (passed) and/or `analyzed` (flagged). Every processed published node gets a record — `ok` for passes, `analyzed` for flags.

## 3. Listing Page

**URL:** `/admin/content/ai-content-lifecycle`  
**Permission:** `view content lifecycle data`

Verify:
- Listing loads without errors.
- Summary bar shows counts per status.
- Filter by `analyzed` shows only flagged items.
- Filter by `ok` shows only passing items.
- Sorting by column header works.
- Pager appears when more than 50 items exist.

## 4. Detail and Status Update

Click **View** on any listing item.

Verify:
- Content title, type, status, and dates display correctly.
- AI Analysis section shows the explanation (for `analyzed` records).
- **View content** and **Edit content** links work.
- Status form updates `review_status` and redirects to the listing.

## 5. Auto-Queue on Save

Enable "Auto-queue on save" in settings. Edit and save a published node of an enabled type. Verify a queue item is created:

```bash
ddev bee php-script - <<'PHP'
$queue = BackdropQueue::get('ai_content_lifecycle_analysis');
$info = $queue->numberOfItems();
echo "Queue depth: $info\n";
PHP
```

Run cron or trigger the queue worker manually and confirm the node gets a lifecycle record.

## 6. Manual PHP Tests

### Create a lifecycle entity manually
```bash
ddev bee php-script - <<'PHP'
$node = node_load(1);
if ($node) {
  $lifecycle = entity_create('content_lifecycle', []);
  $lifecycle->setReferencedEntity($node);
  $lifecycle->ai_results = 'Test result.';
  $lifecycle->review_status = 'analyzed';
  $lifecycle->save();
  echo 'Created lifecycle ID: ' . $lifecycle->id . "\n";
}
PHP
```

### Run the analyzer directly
```bash
ddev bee php-script - <<'PHP'
$node = node_load(1);
if ($node) {
  $analyzer = new AIContentLifecycleAnalyzer();
  $result = $analyzer->analyzeContent($node);
  echo $result ? "Flagged: $result\n" : "OK — no issues found.\n";
}
PHP
```

## Troubleshooting

### Check watchdog logs
```bash
ddev bee watchdog-show --count=20
```

### Verify AI provider config
```bash
ddev bee config-get ai.settings providers
```

### Verify module config
```bash
ddev bee config-get ai_content_lifecycle.settings
```

### Reinstall if schema is broken
```bash
ddev bee pm-disable ai_content_lifecycle -y
ddev bee pm-uninstall ai_content_lifecycle -y
ddev bee pm-enable ai_content_lifecycle -y
```

## Performance Notes

- Each content analysis makes up to 2 AI provider calls (detection + explanation for flagged items; 1 call for items that pass).
- Recommended: start with a small content type to gauge cost before enabling everything.
- Monitor provider usage and adjust `cron_batch_size` in settings if needed.