# Architecture Decision Records (ADR)

This directory contains Architecture Decision Records for the MakineAI project.

## What is an ADR?

An Architecture Decision Record (ADR) is a document that captures an important architectural decision made along with its context and consequences.

## ADR Template

Each ADR follows this structure:

```markdown
# ADR-NNNN: Title

## Status
[Proposed | Accepted | Deprecated | Superseded by ADR-XXXX]

## Context
What is the issue that we're seeing that is motivating this decision?

## Decision
What is the change that we're proposing and/or doing?

## Consequences
What becomes easier or more difficult to do because of this change?

## Alternatives Considered
What other options were considered and why were they rejected?
```

## ADR Index

| ID | Title | Status | Date |
|----|-------|--------|------|
| [ADR-0001](0001-native-cpp-architecture.md) | Native C++ Architecture | Accepted | 2026-01 |
| [ADR-0002](0002-result-based-error-handling.md) | Result-Based Error Handling | Accepted | 2026-01 |
| [ADR-0003](0003-translation-pipeline-decision-engine.md) | Translation Pipeline Decision Engine | Accepted | 2026-01 |
| [ADR-0004](0004-optional-library-integration.md) | Optional Library Integration Pattern | Accepted | 2026-01 |
| [ADR-0005](0005-handler-based-engine-support.md) | Handler-Based Engine Support | Accepted | 2026-01 |

## Creating a New ADR

1. Copy the template from `template.md`
2. Name it `NNNN-short-title.md` (use next available number)
3. Fill in all sections
4. Update this index
5. Submit for review

## References

- [ADR GitHub Organization](https://adr.github.io/)
- [Michael Nygard's original article](https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions)
