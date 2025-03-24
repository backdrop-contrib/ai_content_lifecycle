## INTRODUCTION

The AI Content Lifecycle module provides AI-powered content analysis to help content editors review and improve content quality on their Drupal site.

The primary use cases for this module are:

- Automating content quality assessment using AI analysis
- Creating a workflow for content editors to review AI-analyzed content
- Tracking content review statuses (pending, analyzed, reviewed, updated, ignored)

## REQUIREMENTS

- Drupal 10 or 11
- An AI service for content analysis (implementation dependent)

## INSTALLATION

Install as you would normally install a contributed Drupal module.
See: https://www.drupal.org/node/895232 for further information.

## CONFIGURATION

1. Navigate to Configuration > AI  > AI Lifecycle settings
2. Configure which content types and bundles should be analyzed
3. Set up AI analysis parameters
4. Use the batch process tool to analyze existing content
5. New content will be automatically analyzed upon creation

## VIEWS INTEGRATION

The module provides Views integration with relationships to connect content lifecycle entities to their referenced content entities (nodes, media, taxonomy terms, etc.).

## MAINTAINERS

Current maintainers for Drupal 10:

- ayalon - https://www.drupal.org/u/ayalon
