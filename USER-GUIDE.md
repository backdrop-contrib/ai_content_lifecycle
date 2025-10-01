# OpenAI Content Lifecycle - User Guide

## Where to Find Flagged Content

### Main Listing Page
**URL:** `/admin/content/content-lifecycle`  
**Menu:** Admin → Content → Content Lifecycle

This page shows all content that has been flagged by AI for review.

### What You'll See

The listing page includes:

1. **Summary Bar** - Quick stats showing total count and breakdown by status
2. **Filter** - Filter by review status (Pending, Analyzed, Reviewed, etc.)
3. **Content Table** with columns:
   - **Content** - Title and link to the content
   - **Type** - Entity type (e.g., node)
   - **Status** - Current review status with color coding
   - **AI Analysis** - Preview of why it was flagged (first 100 chars)
   - **Created** - When the analysis was performed
   - **Operations** - Links to view details or edit content

### Status Colors

- **Pending** (Blue) - Awaiting analysis
- **Analyzed** (Orange) - Flagged by AI, needs human review
- **Reviewed** (Green) - Human reviewed the analysis
- **Updated** (Green, Bold) - Content has been updated
- **Ignored** (Gray) - Marked as not needing changes

## Workflow

### 1. Run Analysis
- Go to: Configuration → Content → OpenAI Content Lifecycle
- Configure your prompts and content types
- Click "Analyze Existing Content"
- Wait for batch process to complete

### 2. Review Flagged Content
- Navigate to: Admin → Content → Content Lifecycle
- You'll see all content flagged by AI
- Filter by "Analyzed" status to see items needing review

### 3. View Details
- Click "View" on any item to see:
  - Full AI analysis explanation
  - Link to view/edit the actual content
  - Ability to update the review status

### 4. Take Action
For each flagged item, you can:
- **View content** - See what was flagged
- **Edit content** - Make necessary updates
- **Update status** - Mark as reviewed, updated, or ignored

### 5. Track Progress
Use the filters and summary stats to:
- See how many items are pending review
- Track items that have been reviewed
- Monitor updated vs. ignored items

## Menu Locations

### Content Management
- **Admin → Content → Content Lifecycle** - Main listing page
  - Permission needed: "View content lifecycle data"

### Configuration
- **Admin → Config → Content → OpenAI Content Lifecycle** - Settings
  - Permission needed: "Administer OpenAI Content Lifecycle"

## Permissions

Three permission levels:

1. **Administer OpenAI Content Lifecycle**
   - Configure settings
   - Run batch analysis
   - Full access

2. **View content lifecycle data**
   - View the flagged content list
   - See AI analysis results
   - View details

3. **Edit content lifecycle data**
   - Change review status
   - Mark items as reviewed/ignored
   - Update lifecycle entities

## Quick Start

1. **Configure** (one-time setup):
   ```
   /admin/config/content/openai-content-lifecycle
   ```
   - Set your prompts
   - Choose content types to analyze

2. **Analyze** (run as needed):
   ```
   /admin/config/content/openai-content-lifecycle/batch
   ```
   - Processes all existing content
   - Creates lifecycle entities for flagged items

3. **Review** (ongoing):
   ```
   /admin/content/content-lifecycle
   ```
   - Check flagged content
   - Update as needed
   - Track your progress

## Tips

### Filtering Results
Use the status filter to focus on specific items:
- **Analyzed** - Start here for new items to review
- **Reviewed** - Items you've looked at but not updated yet
- **Pending** - Items waiting for initial analysis

### Batch Processing
- Run during off-peak hours if you have lots of content
- Monitor your OpenAI API usage/costs
- Process in smaller chunks by enabling fewer content types at once

### Status Updates
Recommended workflow:
1. AI flags content → Status: **Analyzed**
2. Editor reviews → Update to: **Reviewed**
3. Editor updates content → Update to: **Updated**
4. Or if no changes needed → Update to: **Ignored**

### Search & Sort
- Click column headers to sort the list
- Use browser search (Ctrl+F) to find specific content
- Pager shows 50 items per page

## Troubleshooting

### "No content has been flagged"
- Run the batch analysis first
- Check your OpenAI API key is configured
- Verify content types are enabled in settings

### Can't access listing page
- Check you have "View content lifecycle data" permission
- Make sure module is enabled
- Clear cache if menu item doesn't appear

### AI analysis not running
- Verify OpenAI module is configured
- Check API key has credits
- Look at watchdog logs for errors

## Next Steps

### Create a View (Optional)
For more advanced reporting, you can create a custom View:
- Base: Content Lifecycle
- Add relationships to referenced content
- Create custom filters and displays
- Export data as needed

### Integrate with Workflow
- Use status updates in your editorial workflow
- Set up notifications (via Rules module if available)
- Create reports for content quality metrics

### Monitor & Improve
- Review what AI flags most often
- Refine your prompts based on results
- Adjust content type selection as needed
- Track time saved identifying outdated content

## Support

For issues or questions:
1. Check watchdog logs: `/admin/reports/dblog`
2. Review module documentation files
3. Verify OpenAI API status
4. Check module status page: `/admin/reports/status`
