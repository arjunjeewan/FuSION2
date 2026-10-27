# FuSION 2.0: rules for Claude

FuSION 2.0 is a full rewrite of FuSION 1 (read-only
reference at /home/ks-149-u24/Desktop/arjun/FuSION).
Backend in C++, frontend in Spring Boot (Java).

## Process
- Poll the user before every decision, recommended
  option first. Never decide silently or fold several
  decisions into one step.
- Log every user prompt word for word, every poll with
  its options and answer, and the full text of every
  subagent prompt in docs/process/PROMPTS.md. Append
  only. (decision 0002)
- Give every decision a record in docs/decisions/ and
  a row in docs/decisions/README.md.
- Work through the phases in order. A phase ends only
  after the user reviews it. (decision 0003)
- Show the diff and get approval before every commit.
  Push only when the user says so. (decision 0001)

## Scope
- Port only what the current FuSION 1 build runs. Code
  in FuSION 1's #if 0 regions is out of scope.
  (decision 0004)
