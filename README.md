# AI Content Lifecycle

This module provides AI-powered content analysis to help content editors identify and review outdated or inaccurate content on their Backdrop CMS site.

## Features

- **Automated Content Analysis**: Uses AI's GPT models to analyze content based on configurable conditions
- **Flexible Configuration**: Set up different analysis criteria for different content types
- **Batch Processing**: Analyze all existing content at once or process content automatically as it's created
- **Custom Prompts**: Configure exactly what conditions should trigger a review flag

## Requirements

- Backdrop CMS 1.x
- AI module (with configured API key)
- Key module (for secure API key storage)

## Installation

1. Install and enable the AI module and configure your API key
2. Install and enable this module
3. Visit Configuration > Content > AI Content Lifecycle to configure

## Configuration

1. Navigate to **Configuration > Content > AI Content Lifecycle**
2. Select which AI model to use (GPT-4o Mini is recommended for cost-effectiveness)
3. Define your default analysis conditions (e.g., "mentions outdated information")
4. Select which content types should be analyzed
5. Optionally set specific conditions for each content type
6. Save configuration

## Usage

### Batch Analysis

After configuring the module, use the "Analyze Existing Content" button to process all published content of the selected types. The module will:

1. Extract text content from each node
2. Send it to AI with your configured prompts
3. Flag content that needs review based on the AI's analysis
4. Store the analysis results for editor review

### Viewing Results

Analysis results are stored and can be accessed through:
- State API: `state_get('ai_content_lifecycle_' . $nid)`
- Integration with Flag module (if enabled) - creates "needs_review" flags

## How It Works

The module performs a two-step analysis:

1. **Initial Check**: Asks AI if content meets the configured conditions for review (responds with XTRUE/XFALSE)
2. **Detailed Analysis**: If flagged, requests a detailed explanation of why the content needs review

This approach minimizes API costs while providing useful feedback to content editors.

## Extending

### Hooks

**hook_ai_content_lifecycle_prepare_content($entity)**
- Prepare custom content extraction for entities
- Return a string of content to analyze

**hook_ai_content_lifecycle_prompt_alter(&$prompt, $entity)**
- Modify the prompt before sending to AI
- Add custom tokens or context

**hook_ai_content_lifecycle_content_alter(&$content, $entity)**
- Modify the extracted content array before analysis
- Keys: 'title', 'content', 'updated', 'language'

## Credits

This module is a Backdrop CMS port of the Drupal "AI Content Lifecycle" module, adapted to work with the AI module instead of the Drupal AI module.

Original Drupal module maintainers:
- ayalon
- wouters_f

Backdrop port: Adapted for Backdrop CMS with AI module integration

- Developed with AI assistance.

## License

GPL-2.0-or-later

