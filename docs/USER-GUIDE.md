# AI Content Lifecycle - User Guide

## Where to Find Flagged Content

### Main Listing Page
**URL:** `/admin/content/ai-content-lifecycle`  
**Menu:** Admin → Content → Content Lifecycle

This page shows all content that has been analyzed by AI.

### What You'll See

The listing page includes:

1. **Summary Bar** — Quick stats showing total count and per-status breakdown, with links to filter by status.
2. **Filter** — Filter by review status.
3. **Content Table** with columns:
   - **Content** — Title linked to the content.
   - **Type** — Content bundle (e.g., Article, Page).
   - **Status** — Current review status.
   - **AI Analysis** — Preview of why it was flagged (first 100 characters).
   - **Created** — When the analysis was performed.
   - **Operations** — View detail or edit the content directly.

### Review Statuses

| Status | Set by | Meaning |
|---|---|---|
| `pending` | System | Item queued but not yet analyzed. |
| `ok` | System | AI found no issues; content passed. |
| `analyzed` | System | AI flagged this content for review. |
| `reviewed` | Editor | Editor has looked at the AI analysis. |
| `updated` | Editor | Editor updated the content. |
| `ignored` | Editor | Editor decided no changes are needed. |

## Workflow

### 1. Configure
Go to **Admin → Configuration → AI → AI Content Lifecycle** and:
- Set the AI model and temperature.
- Write the conditions that should trigger a flag (e.g., "contains outdated statistics").
- Select which content types to analyze.

### 2. Analyze
Run the batch at:
```
/admin/config/ai/ai-content-lifecycle/batch
```
The batch processes all published nodes of enabled types. Each node gets a lifecycle record:
- AI flags it → `review_status = analyzed`, AI explanation stored.
- AI passes it → `review_status = ok`, no explanation.

New and updated nodes are queued automatically on save (if auto-queue is enabled in settings).

### 3. Review Flagged Content
- Go to **Admin → Content → Content Lifecycle**.
- Filter by `analyzed` to see items needing attention.
- Click **View** on any item to see the full AI explanation and a link to edit the content.

### 4. Update Status
On the detail page, use the **Update Status** form to move items through your workflow:

1. AI flags content → `analyzed`
2. Editor reads the analysis → set to `reviewed`
3. Editor updates the content → set to `updated`
4. Or if no changes are needed → set to `ignored`

## Permissions

| Permission | Access |
|---|---|
| `administer ai content lifecycle` | Configure settings, run batch, full access. |
| `view content lifecycle data` | View the listing and detail pages. |
| `edit content lifecycle data` | Update review status on lifecycle entities. |

## Troubleshooting

**"No content has been flagged for review yet"**
— Run the batch analysis first. Check that content types are enabled in settings and the AI provider is configured.

**Can't access the listing page**
— Verify you have the `view content lifecycle data` permission and the module is enabled. Clear caches if the menu item is missing.

**AI analysis not running**
— Check that the AI module has a provider and API key configured. Check watchdog logs at `/admin/reports/dblog` for provider errors.
