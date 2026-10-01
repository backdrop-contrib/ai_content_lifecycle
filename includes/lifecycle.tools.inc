<?php

/**
 * @file
 * Tool definitions and execution callbacks for ai_content_lifecycle agent tools.
 */

/**
 * Returns tool definitions for hook_ai_tools().
 */
function ai_content_lifecycle_tools_definitions() {
  $tools = [];

  $tools['get_content_lifecycle_summary'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'get_content_lifecycle_summary',
        'description' => 'Returns aggregate counts and metrics for content lifecycle review statuses across the site (e.g. analyzed/flagged, ok, pending, reviewed, updated, ignored, and total).',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'bundle' => [
              'type' => 'string',
              'description' => 'Optional content type machine name (e.g. article, post) to restrict metrics.',
            ],
          ],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_content_lifecycle_tool_get_summary',
    'operation' => 'read',
    'permission' => 'view content lifecycle data',
    'module' => 'ai_content_lifecycle',
    'category' => 'content',
  ];

  $tools['list_flagged_content'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'list_flagged_content',
        'description' => 'Lists published content items flagged by AI as needing review (status "analyzed"), including the AI explanation of why it was flagged, node ID, title, and changed date.',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'bundle' => [
              'type' => 'string',
              'description' => 'Optional content type machine name (e.g. article, post) to restrict results.',
            ],
            'limit' => [
              'type' => 'integer',
              'description' => 'Maximum number of items to return (default 20, max 100).',
            ],
          ],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_content_lifecycle_tool_list_flagged',
    'operation' => 'read',
    'permission' => 'view content lifecycle data',
    'module' => 'ai_content_lifecycle',
    'category' => 'content',
  ];

  $tools['analyze_node_lifecycle'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'analyze_node_lifecycle',
        'description' => 'Runs an on-demand AI content lifecycle analysis on a specific node to evaluate freshness, contradictions, and obsolete content.',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'nid' => [
              'type' => 'integer',
              'description' => 'The node ID to analyze.',
            ],
            'async' => [
              'type' => 'boolean',
              'description' => 'If TRUE and background worker is available, submits the job asynchronously and returns immediately.',
            ],
          ],
          'required' => ['nid'],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_content_lifecycle_tool_analyze_node',
    'operation' => 'write',
    'permission' => 'edit content lifecycle data',
    'module' => 'ai_content_lifecycle',
    'category' => 'content',
  ];

  return $tools;
}

/**
 * Tool execution callback: get_content_lifecycle_summary.
 */
function ai_content_lifecycle_tool_get_summary(array $parameters = []) {
  if (!_ai_content_lifecycle_table_exists()) {
    return ['error' => 'The content_lifecycle table does not exist.'];
  }

  $bundle = isset($parameters['bundle']) ? trim((string) $parameters['bundle']) : '';

  $query = db_select('content_lifecycle', 'c')
    ->fields('c', ['review_status']);
  $query->addExpression('COUNT(*)', 'count');
  $query->groupBy('review_status');

  if ($bundle !== '') {
    $query->condition('entity_bundle', $bundle);
  }

  $rows = $query->execute()->fetchAllKeyed();

  $statuses = ['analyzed', 'ok', 'pending', 'reviewed', 'updated', 'ignored'];
  $summary = [];
  $total = 0;
  foreach ($statuses as $st) {
    $count = isset($rows[$st]) ? (int) $rows[$st] : 0;
    $summary[$st] = $count;
    $total += $count;
  }
  $summary['total'] = $total;

  return [
    'bundle' => $bundle ?: 'all',
    'metrics' => $summary,
    'needs_review_count' => $summary['analyzed'],
  ];
}

/**
 * Tool execution callback: list_flagged_content.
 */
function ai_content_lifecycle_tool_list_flagged(array $parameters = []) {
  if (!_ai_content_lifecycle_table_exists()) {
    return ['error' => 'The content_lifecycle table does not exist.'];
  }

  $bundle = isset($parameters['bundle']) ? trim((string) $parameters['bundle']) : '';
  $limit = isset($parameters['limit']) ? max(1, min((int) $parameters['limit'], 100)) : 20;

  $query = db_select('content_lifecycle', 'c')
    ->fields('c', ['id', 'entity_type', 'entity_id', 'entity_bundle', 'label', 'review_status', 'ai_results', 'created', 'changed'])
    ->condition('c.review_status', 'analyzed')
    ->orderBy('c.changed', 'DESC')
    ->range(0, $limit);

  if ($bundle !== '') {
    $query->condition('c.entity_bundle', $bundle);
  }

  $records = $query->execute()->fetchAll();

  $items = [];
  foreach ($records as $record) {
    $url = '';
    if ($record->entity_type === 'node' && !empty($record->entity_id)) {
      $url = url('node/' . $record->entity_id, ['absolute' => TRUE]);
    }
    $items[] = [
      'lifecycle_id' => (int) $record->id,
      'entity_type' => $record->entity_type,
      'nid' => (int) $record->entity_id,
      'bundle' => $record->entity_bundle,
      'title' => $record->label,
      'ai_explanation' => $record->ai_results,
      'changed' => format_date($record->changed, 'short'),
      'url' => $url,
      'lifecycle_url' => url('admin/content/ai-content-lifecycle/' . $record->id, ['absolute' => TRUE]),
    ];
  }

  return [
    'bundle' => $bundle ?: 'all',
    'count' => count($items),
    'items' => $items,
  ];
}

/**
 * Tool execution callback: analyze_node_lifecycle.
 */
function ai_content_lifecycle_tool_analyze_node(array $parameters) {
  $nid = isset($parameters['nid']) ? (int) $parameters['nid'] : 0;
  if ($nid <= 0) {
    return ['error' => 'A valid positive integer nid is required.'];
  }

  $node = node_load($nid);
  if (!$node) {
    return ['error' => "Node $nid not found."];
  }

  if (!node_access('view', $node)) {
    return ['error' => "Access denied to node $nid."];
  }

  $async = !empty($parameters['async']);
  if ($async && module_exists('ai_async') && function_exists('ai_async_submit')) {
    $job_id = ai_async_submit('ai_content_lifecycle_async_analyze', ['nid' => $nid]);
    return [
      'status' => TRUE,
      'async' => TRUE,
      'job_id' => $job_id,
      'nid' => $nid,
      'message' => "Node $nid analysis queued in the background.",
    ];
  }

  try {
    $analyzer = new AIContentLifecycleAnalyzer();
    $lifecycle = $analyzer->updateContentLifecycleWithAnalysis($node, NULL);

    if (!$lifecycle) {
      return [
        'status' => FALSE,
        'nid' => $nid,
        'error' => 'Analysis failed (API error or empty response). Check watchdog logs for details.',
      ];
    }

    return [
      'status' => TRUE,
      'nid' => $nid,
      'title' => $node->title,
      'review_status' => $lifecycle->review_status,
      'flagged' => ($lifecycle->review_status === 'analyzed'),
      'ai_results' => $lifecycle->ai_results,
      'lifecycle_id' => (int) $lifecycle->id,
      'message' => ($lifecycle->review_status === 'analyzed')
        ? 'Content was analyzed and flagged for review.'
        : 'Content was analyzed and passed freshness and quality checks.',
    ];
  }
  catch (\Exception $e) {
    return [
      'status' => FALSE,
      'nid' => $nid,
      'error' => $e->getMessage(),
    ];
  }
}
