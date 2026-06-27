<?php

/**
 * @file
 * AI Content Lifecycle Analyzer class for Backdrop CMS.
 */

// Prefer Composer Manager's autoloader when available; fall back to module vendor.
$__ai_sdk_source =& backdrop_static('ai_sdk_source');
if (module_exists('composer_manager')) {
  if (function_exists('composer_manager_register_autoloader')) {
    composer_manager_register_autoloader();
  }
  $__ai_sdk_source = 'composer_manager';
}
else {
  $autoload = BACKDROP_ROOT . '/vendor/autoload.php';
  if (file_exists($autoload)) {
    require_once $autoload;
    $__ai_sdk_source = 'module_vendor';
  } else {
    $__ai_sdk_source = $__ai_sdk_source ?: 'unknown';
  }
}

/**
 * Service for performing AI content analysis on entities.
 */
class AIContentLifecycleAnalyzer {

  /**
   * Default system prompt for content analysis.
   */
  private $technicalSystemPrompt = '';

  /**
   * The AI API client.
   *
   * @var AIApi
   */
  protected $aiApi;
  protected $model;

  /**
   * Constructor.
   */
  public function __construct() {
    // Get configuration.
    $config = config('ai_content_lifecycle.settings');

    // Get model from config.
    $this->model = $config->get('default_model');
    if (empty($this->model)) {
      $this->model = config_get('ai.settings', 'default_text_model') ?: 'gpt-4o-mini';
    }

    // Get API instance for the configured model.
    $stripped_model = '';
    $this->aiApi = ai_get_api_for_model($this->model, $stripped_model);

    if (!$this->aiApi) {
      watchdog('ai_content_lifecycle', 'AI provider is not configured for model @model.', ['@model' => $this->model], WATCHDOG_ERROR);
      return;
    }
  }

  /**
   * Analyzes content using AI and returns the analysis results.
   *
   * @param object $entity
   *   The entity to analyze.
   *
   * @return string|null|false
   *   The AI analysis explanation when content needs review, NULL when the
   *   content passed the check, or FALSE when analysis failed (API error or
   *   empty model response) — callers must not record FALSE as "ok".
   */
  public function analyzeContent($entity) {
    $config = config('ai_content_lifecycle.settings');

    // Extract content from entity.
    $content = $this->extractEntityContent($entity);

    // Get prompt based on entity type/bundle.
    $entity_prompt = $this->getPromptForEntity($entity);
    $pre_prompt = $config->get('pre_prompt');
    if (empty($pre_prompt)) {
      $pre_prompt = $this->getDefaultSystemPrompt();
    }

    $total_prompt = $pre_prompt . $this->technicalSystemPrompt;
    $prompt = str_replace('[conditions]', $entity_prompt, $total_prompt);

    // Get current date in d.m.Y format.
    $date = format_date(REQUEST_TIME, 'custom', 'd.m.Y');
    $prompt = str_replace('[ai_content_lifecycle:date]', $date, $prompt);

    $language = language_default();
    $prompt = str_replace('[lang]', $language->name, $prompt);
    $prompt = str_replace('[context]', $content['title'] . "\n\n" . $content['content'], $prompt);

    // Allow other modules to alter the prompt.
    backdrop_alter('ai_content_lifecycle_prompt', $prompt, $entity);

    // Add similarity context from Search API if configured.
    $similarity_context = $this->buildSimilarityContext($content, $entity);
    if ($similarity_context !== '') {
      $prompt .= "\n\n" . $similarity_context;
    }

    // Prepare messages for the provider-neutral AI API.
    $messages = [];

    $site_context = $this->getSiteContextSystemMessage();
    if ($site_context !== '') {
      $messages[] = [
        'role' => 'system',
        'content' => $site_context,
      ];
    }

    $messages[] = [
      'role' => 'system',
      'content' => 'You are an agent that responds with only XTRUE or XFALSE. You evaluate CONTEXT based on the INSTRUCTION the user provides. You return (XTRUE) if the INSTRUCTION provided indicate that the content should be updated or (XFALSE) if not.',
    ];

    $messages[] = [
      'role' => 'user',
      'content' => $prompt,
    ];

    $temperature = $config->get('temperature');
    if (empty($temperature) && $temperature !== 0) {
      $temperature = 0.7;
    }
    // Ensure temperature is a float.
    $temperature = floatval($temperature);

    try {
      // Call the centralized AI API helper.
      $response = ai_chat($this->model, $messages, $temperature, 500, FALSE);

      if (empty($response)) {
        // No verdict — distinct from a clean XFALSE pass.
        return FALSE;
      }

      $result = is_string($response) ? $response : (isset($response->content) ? $response->content : '');

      if (stripos($result, 'XTRUE') !== FALSE) {
        // Content needs review, get detailed explanation.
        $subPrompt = "
VARIABLES
----------
Today is [ai_content_lifecycle:date]

INSTRUCTION
-----------
The below content was marked for review (context).
The content should be marked when
[conditions]

Describe in [lang] in one or two sentences which sentences triggered you to mark this for reviewing always referring to the conditions above.

CONTEXT
-------
[context]
";
        $subPrompt = str_replace('[ai_content_lifecycle:date]', $date, $subPrompt);
        $subPrompt = str_replace('[lang]', $language->name, $subPrompt);
        $subPrompt = str_replace('[context]', $content['title'] . "\n\n" . $content['content'], $subPrompt);
        $subPrompt = str_replace('[conditions]', $entity_prompt, $subPrompt);

        $messages2 = [];

        if ($site_context !== '') {
          $messages2[] = [
            'role' => 'system',
            'content' => $site_context,
          ];
        }

        $messages2[] = [
          'role' => 'system',
          'content' => 'You are a content evaluator, helping web editors. You describe why content should be updated.',
        ];

        $messages2[] = [
          'role' => 'user',
          'content' => $subPrompt,
        ];

        try {
          $response2 = ai_chat($this->model, $messages2, $temperature, 500, FALSE);
          $explanation = is_string($response2) ? $response2 : (isset($response2->content) ? $response2->content : '');
        }
        catch (\Exception $e) {
          watchdog('ai_content_lifecycle', 'Error fetching explanation after XTRUE verdict: @message', ['@message' => $e->getMessage()], WATCHDOG_WARNING);
          $explanation = '';
        }
        // The verdict was XTRUE — never let a failed explanation call turn a
        // flagged result back into an empty (pass-like) value.
        return trim($explanation) !== '' ? $explanation : t('Flagged for review (the model did not return an explanation).');
      }

      return NULL;
    }
    catch (\Exception $e) {
      watchdog('ai_content_lifecycle', 'Error analyzing content with AI: @message', ['@message' => $e->getMessage()], WATCHDOG_ERROR);
      return FALSE;
    }
  }

  /**
   * Gets the appropriate prompt for the entity based on its type and bundle.
   *
   * @param object $entity
   *   The entity to get the prompt for.
   *
   * @return string
   *   The prompt to use for content analysis.
   */
  protected function getPromptForEntity($entity) {
    $config = config('ai_content_lifecycle.settings');
    $bundle_prompts = $config->get('bundle_prompts');
    if (empty($bundle_prompts)) {
      $bundle_prompts = [];
    }

    $entity_type = $entity->entityType();
    $bundle = isset($entity->type) ? $entity->type : $entity_type;

    // Try to get bundle-specific prompt.
    if (isset($bundle_prompts[$entity_type][$bundle])) {
      return is_array($bundle_prompts[$entity_type][$bundle])
        ? $bundle_prompts[$entity_type][$bundle]['value']
        : $bundle_prompts[$entity_type][$bundle];
    }

    // Try entity-type prompt.
    if (isset($bundle_prompts[$entity_type]['default'])) {
      return is_array($bundle_prompts[$entity_type]['default'])
        ? $bundle_prompts[$entity_type]['default']['value']
        : $bundle_prompts[$entity_type]['default'];
    }

    // Default prompt from configuration.
    $default = $config->get('default_prompt');
    if (empty($default)) {
      $default = $this->getDefaultPrompt();
    }
    return $default;
  }

  /**
   * Extracts relevant content from an entity for AI analysis.
   *
   * @param object $entity
   *   The entity to extract content from.
   *
   * @return array
   *   The extracted content with 'title' and 'content' keys.
   */
  protected function extractEntityContent($entity) {
    $content = [];
    $config = config('ai_content_lifecycle.settings');

    // Get entity label/title.
    if (isset($entity->title)) {
      $content['title'] = $entity->title;
    }
    elseif (isset($entity->name)) {
      $content['title'] = $entity->name;
    }
    else {
      $content['title'] = '';
    }

    // Allow other modules to prepare content.
    $content['content'] = '';
    foreach (module_implements('ai_content_lifecycle_prepare_content') as $module) {
      $function = $module . '_ai_content_lifecycle_prepare_content';
      $result = $function($entity);
      if (!empty($result)) {
        $content['content'] .= $result . "\n\n";
      }
    }

    // If no content was prepared, extract from fields.
    if (empty(trim($content['content']))) {
      $content['content'] = $this->extractTextFieldsContent($entity);
    }

    // Clean up content.
    $content['content'] = trim(preg_replace('/\n\s*\n/', "\n", $content['content']));

    // Get updated date if available.
    $content['updated'] = NULL;
    if (isset($entity->changed)) {
      $content['updated'] = format_date($entity->changed, 'medium');
    }

    // Get language.
    $default_language = language_default();
    $content['language'] = $default_language->langcode;
    if (isset($entity->langcode)) {
      $content['language'] = $entity->langcode;
    }

    // Allow other modules to alter the content.
    backdrop_alter('ai_content_lifecycle_content', $content, $entity);

    return $content;
  }

  /**
   * Extract content from text fields as a fallback method.
   *
   * @param object $entity
   *   The entity to extract content from.
   *
   * @return string
   *   The extracted text content.
   */
  protected function extractTextFieldsContent($entity) {
    $entity_type = method_exists($entity, 'entityType') ? $entity->entityType() : 'node';
    return AIContentExtractor::extractEntityText($entity_type, $entity, [
      'view_mode' => 'full',
      'include_label' => FALSE,
    ]);
  }

  /**
   * Build a similarity context section using configured mode.
   */
  protected function buildSimilarityContext(array $content, $entity) {
    $config = config('ai_content_lifecycle.settings');
    $mode = $this->getSimilarityMode($config);

    if ($mode === 'vector') {
      $vector_context = $this->buildVectorSimilarityContext($content, $entity, $config);
      if ($vector_context !== '') {
        return $vector_context;
      }
      // Fall back to Search API if vector search fails.
      $mode = 'search_api';
    }

    if ($mode === 'search_api') {
      return $this->buildSearchApiSimilarityContext($content, $entity, $config);
    }

    return '';
  }

  /**
   * Build a similarity context section using Search API results.
   */
  protected function buildSearchApiSimilarityContext(array $content, $entity, $config) {
    $index_id = $config->get('similarity_index');

    if (empty($index_id) || !module_exists('search_api')) {
      return '';
    }

    $index = search_api_index_load($index_id);
    if (!$index) {
      return '';
    }

    try {
      $query = search_api_query($index_id);
    }
    catch (\Exception $e) {
      watchdog('ai_content_lifecycle', 'Failed to build similarity query: @error', ['@error' => $e->getMessage()], WATCHDOG_WARNING);
      return '';
    }

    $keywords = trim(strip_tags($content['title'] . ' ' . $content['content']));
    if (function_exists('backdrop_substr')) {
      $keywords = backdrop_substr($keywords, 0, 500);
    }
    else {
      $keywords = substr($keywords, 0, 500);
    }

    if ($keywords !== '') {
      $query->keys($keywords);
    }

    $query->range(0, 5);

    try {
      $results = $query->execute();
    }
    catch (\Exception $e) {
      watchdog('ai_content_lifecycle', 'Similarity lookup failed for index @index: @error', ['@index' => $index_id, '@error' => $e->getMessage()], WATCHDOG_WARNING);
      return '';
    }

    if (empty($results['results']) || !is_array($results['results'])) {
      return '';
    }

    $lines = [];
    foreach ($results['results'] as $item_id => $item) {
      $score = NULL;
      if (is_array($item) && isset($item['score'])) {
        $score = $item['score'];
      }
      elseif (is_object($item) && isset($item->score)) {
        $score = $item->score;
      }

      $title = '';
      $url = '';
      $snippet = '';
      $nid = NULL;

      if (is_string($item_id) && preg_match('/node[:\/](\d+)/', $item_id, $matches)) {
        $nid = intval($matches[1]);
      }

      // Skip current entity if we can detect the same node id.
      if ($nid && isset($entity->nid) && intval($entity->nid) === $nid) {
        continue;
      }

      if ($nid) {
        $node = node_load($nid);
        if ($node) {
          $title = isset($node->title) ? $node->title : t('Node @nid', ['@nid' => $nid]);
          $url = url('node/' . $nid, ['absolute' => TRUE]);
          $snippet_source = $this->extractTextFieldsContent($node);
          if (function_exists('backdrop_substr')) {
            $snippet = backdrop_substr(trim($snippet_source), 0, 280);
          }
          else {
            $snippet = substr(trim($snippet_source), 0, 280);
          }
        }
      }

      if (empty($title) && is_array($item) && isset($item['fields']['title']['value'])) {
        $title = is_array($item['fields']['title']['value']) ? reset($item['fields']['title']['value']) : $item['fields']['title']['value'];
      }

      if (empty($title)) {
        $title = is_string($item_id) ? $item_id : t('Similar item');
      }

      $line = '- ' . $title;
      if (!empty($url)) {
        $line .= ' (' . $url . ')';
      }
      if ($score !== NULL) {
        $line .= ' [score ' . round($score, 3) . ']';
      }
      if (!empty($snippet)) {
        $line .= "\n  " . $snippet;
      }

      $lines[] = $line;
    }

    if (empty($lines)) {
      return '';
    }

    return t('Similar content candidates (index: @index):', ['@index' => $index->name]) . "\n" . implode("\n", $lines);
  }

  /**
   * Determine similarity mode with safe fallback.
   */
  protected function getSimilarityMode($config) {
    $mode = $config->get('similarity_mode');
    if (empty($mode)) {
      $mode = !empty($config->get('similarity_index')) ? 'search_api' : 'none';
    }
    return $mode;
  }

  /**
   * Build similarity context using the existing ai_search RAG index.
   *
   * Queries the configured ai_search index directly — no separate
   * embedding generation or collection management needed.
   */
  protected function buildVectorSimilarityContext(array $content, $entity, $config) {
    $index_id = $config->get('vector_index_id');

    if (empty($index_id) || !module_exists('ai_search') || !function_exists('ai_search_execute_rag_search')) {
      return '';
    }

    module_load_include('inc', 'ai_search', 'includes/rag.tools');

    $top_k = (int) ($config->get('vector_top_k') ?: 5);
    $threshold = $config->get('vector_score_threshold');
    $score_threshold = ($threshold !== '' && $threshold !== NULL && is_numeric($threshold)) ? (float) $threshold : 0.0;

    try {
      $result = ai_search_execute_rag_search([
        'query'           => $content['title'],
        'index_ids'       => [$index_id],
        'top_k'           => $top_k,
        'score_threshold' => $score_threshold,
        'entity_types'    => ['node'],
      ]);
    }
    catch (\Exception $e) {
      watchdog('ai_content_lifecycle', 'Vector similarity search failed: @error', ['@error' => $e->getMessage()], WATCHDOG_WARNING);
      return '';
    }

    if (empty($result['sources']) || !is_array($result['sources'])) {
      return '';
    }

    $current_nid = isset($entity->nid) ? (int) $entity->nid : 0;
    $lines = [];

    foreach ($result['sources'] as $source) {
      $match_nid = !empty($source['entity_id']) ? (int) $source['entity_id'] : 0;
      if (!$match_nid || $match_nid === $current_nid) {
        continue;
      }

      $score = isset($source['score']) ? (float) $source['score'] : NULL;
      $title = !empty($source['label']) ? $source['label'] : t('Node @nid', ['@nid' => $match_nid]);
      $url = url('node/' . $match_nid, ['absolute' => TRUE]);

      $snippet = '';
      if (!empty($source['excerpt'])) {
        $snippet = trim(strip_tags($source['excerpt']));
        $snippet = backdrop_substr($snippet, 0, 280);
      }

      $line = '- ' . $title . ' (' . $url . ')';
      if ($score !== NULL) {
        $line .= ' [score ' . round($score, 3) . ']';
      }
      if (!empty($snippet)) {
        $line .= "\n  " . $snippet;
      }

      $lines[] = $line;
    }

    if (empty($lines)) {
      return '';
    }

    return t('Similar content candidates (vector):') . "\n" . implode("\n", $lines);
  }

  /**
   * Get the default system prompt.
   *
   * @return string
   *   The default system prompt.
   */
  protected function getDefaultSystemPrompt() {
    return "
Format:
------
Respond XTRUE or XFALSE, nothing else, no pleasantries or other things.

Variables:
---------
Today is [ai_content_lifecycle:date]

INSTRUCTIONS
------------
return XTRUE when the content
[conditions]

Otherwise return XFALSE

This is the content you have evaluate:
----------------------------------------
[context]
";
  }

  /**
   * Retrieve site context / guardrails system prompt if available.
   */
  protected function getSiteContextSystemMessage() {
    if (module_exists('ai_site_context') && function_exists('ai_site_context_get_prompt')) {
      $site_prompt = trim(ai_site_context_get_prompt());
      if ($site_prompt !== '') {
        return $site_prompt;
      }
    }
    return '';
  }

  /**
   * Get the default prompt conditions.
   *
   * @return string
   *   The default prompt conditions.
   */
  protected function getDefaultPrompt() {
    return "- contains inconsistencies or contradictions\n- appears to be outdated and a human should check it";
  }

  /**
   * Updates a AIContentLifecycle entity with AI analysis results.
   *
   * @param object $entity
   *   The entity to analyze.
   * @param AIContentLifecycle|null $lifecycle
   *   Optional lifecycle entity to update, or NULL to find/create one.
   *
   * @return AIContentLifecycle|null
   *   The updated lifecycle entity or NULL on failure.
   */
  public function updateContentLifecycleWithAnalysis($entity, $lifecycle = NULL) {
    // Find existing or create new lifecycle entity.
    if (!$lifecycle) {
      $existing = AIContentLifecycle::findForEntity($entity);

      if (!empty($existing)) {
        $lifecycle = reset($existing);
      }
      else {
        $lifecycle = entity_create('content_lifecycle', []);
        $lifecycle->setReferencedEntity($entity);
      }
    }

    // Get AI analysis.
    $analysis_result = $this->analyzeContent($entity);

    // Analysis failure (API error / empty response) is not a verdict — do
    // not record 'ok' or clobber a previous result during an outage.
    if ($analysis_result === FALSE) {
      watchdog('ai_content_lifecycle', 'Analysis failed for @type @id; lifecycle status left unchanged.', [
        '@type' => $entity->entityType(),
        '@id' => method_exists($entity, 'id') ? $entity->id() : '?',
      ], WATCHDOG_WARNING);
      return NULL;
    }

    if ($analysis_result) {
      $lifecycle->ai_results = $analysis_result;
      $lifecycle->review_status = 'analyzed';
    }
    else {
      // Content passed — record that it was checked so it appears in cluster views.
      $lifecycle->ai_results = '';
      $lifecycle->review_status = 'ok';
    }

    $lifecycle->save();
    return $lifecycle;
  }

}
