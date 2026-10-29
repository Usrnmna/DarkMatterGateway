# Development conventions

## Source changes

Keep GPIO assignments, electrical limits, configuration validation, command documentation, and firmware behavior synchronized. Configuration values belong in `board_config.hpp`, `sdkconfig.defaults`, or the appliance profile file rather than scattered implementation literals.

Use descriptive names, focused functions, and comments for nontrivial assumptions, state changes, and resource ownership. Documentation and comments use impersonal technical language. Remove document introductions that add no technical information.

Maintain explicit distinctions between source-derived behavior, host test results, target compilation, and physical measurements. Record hardware qualification in `docs/validation.md` with links to the corresponding bench records.

## Documentation and comments

Use impersonal, descriptive technical language. Describe system behavior, requirements, assumptions, and evidence directly. Exclude first-person narration, personal attribution, and development conversations or prompts unless the interaction itself is necessary technical evidence. Legitimate interface terms such as a serial-console prompt and roles such as a device operator retain their technical meanings.

The main [README](README.md) presents the intended audience, practical capabilities, setup path, compatibility, and significant limitations. Detailed protocols, circuits, storage behavior, SDK contracts, and development procedures belong in the relevant topic guide, linked through the [documentation index](docs/README.md). Retain technical information at its maintained location before removing a duplicate explanation from a general document. Use descriptive link labels and verify paths and section anchors after reorganization.

Comments explain nontrivial function contracts, resource ownership, constraints, and reasons for behavior. Document the current implementation without recounting development discussions. Wording-only edits must preserve executable behavior, configuration contracts, and verification limits. Upstream dependencies and generated output retain their source ownership; changes to generated documentation belong in the owning generator.

Use concise diagrams for connections and process flows when they clarify the text. Label proposed hardware and distinguish commands from measured appliance response. Screenshots and photographs require descriptive alternative text and identified test conditions where relevant. Badges must reflect verifiable status; build success does not establish hardware qualification or product certification.

## Verification

Run `bash tools/test-host.sh` after changing portable firmware logic, profile validation, hardware tables, or partition definitions. Build the ESP32-S3 application after changing target code or SDK configuration. The commands and dependency revisions are in the [build and programming guide](docs/build.md).

Storage changes require compatibility analysis for existing versioned records and the `ir_store` partition. Do not add automatic erasure as a recovery path. Startup and recovery must not replay saved appliance commands.

For documentation-only changes, check relative paths, section anchors, fenced blocks, and consistency with the referenced implementation. For comment-only changes, verify that executable code remains unchanged. Record these checks separately from software builds and physical qualification.

## Repository preparation

Keep downloaded SDKs in `.cache/`, builds and exported packages in `build/` or `dist/`, private commissioning material in `private/`, local appliance profiles in `config/*.local.json`, and captured command exports in `captures/`. These locations, generated ESP-IDF configuration, Python environments, environment files, and private-key containers are covered by [.gitignore](.gitignore). An ignore rule does not remove an already-tracked file or identify credentials stored under another filename.

Retain `firmware/dependencies.lock`, `firmware/sdkconfig.defaults`, the disabled configuration examples, test fixtures, and research provenance. [.gitattributes](.gitattributes) uses LF for source and build files, CRLF for PowerShell, and binary handling for documents and images. Retained files under `docs/proflame2/sources/` bypass line-ending conversion so their manifest hashes remain meaningful. [.editorconfig](.editorconfig) follows these conventions and leaves archived references unchanged.

The local checkout is initialized, and its configured `origin` is `https://github.com/Usrnmna/DarkMatterGateway.git`. Confirm the intended destination with `git remote -v` before publication. The existing [LICENSE](LICENSE) contains GNU GPL version 3 text. Third-party references retain their own notices; the [retained-source rights and reuse guidance](docs/proflame2/sources/README.md#rights-and-reuse) identifies publication checks still required for the archived manufacturer PDFs. Repository preparation does not establish permission to redistribute those documents.

Before staging or publishing, inspect `git status --short`, `git ls-files --others --exclude-standard`, and `git ls-files --cached --ignored --exclude-standard`. Review the candidate files for private information, generated output, and third-party publication restrictions. The last command identifies tracked paths that match ignore rules; it should normally produce no output. Changes to licensing, the remote destination, Git history, or publication require separate authorization.

## GitHub verification and maintenance

The single [Host checks workflow](.github/workflows/host-checks.yml) retains the `portable-core-and-configuration` job. Pushes, pull requests, and manual dispatches run the same `bash tools/test-host.sh` entry point on Ubuntu 24.04 with Python 3.12. The job covers Python configuration, hardware-table, serial-client and artifact-export tests, followed by the sanitizer-enabled C++ core, service, storage, Matter/startup, console, and X0A checks described in the [host verification guide](docs/build.md#host-checks).

There are no branch or path filters, so documentation and automation changes receive the same check. New runs cancel superseded runs for the same workflow, event, and Git ref; push and pull-request runs remain separate because they can test different commits. The job has a ten-minute limit, read-only repository permissions, and checkout credential persistence disabled. Action dependencies are pinned to full commit hashes with release comments.

Host checks require no pip packages: serial-client tests replace the serial module with a mock. No dependency or build cache is configured. Firmware compilation remains a separate local check using the [pinned SDK setup and build commands](docs/build.md#dependencies): SDK setup downloads several gigabytes, and target compilation also builds ESP-Matter. The host workflow does not verify target compilation, device operation, or physical safety. Local success does not establish a completed hosted workflow run.

For workflow edits, run `actionlint .github/workflows/host-checks.yml` with a current [actionlint installation](https://github.com/rhysd/actionlint/blob/main/docs/install.md), then run `bash tools/test-host.sh`. Run `git diff --check` and inspect untracked additions separately. Workflow behavior follows the [GitHub Actions syntax reference](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax).

[Dependabot configuration](.github/dependabot.yml) checks action pins and `tools/requirements.txt` weekly, grouping action updates into one pull request. Updates require review; no automatic merge is configured. SDK revisions in the bootstrap script and ESP-IDF managed-component resolution remain manually maintained. Changes to `pyserial` also need the version references in the documentation updated and serial-client validation beyond the mocked host tests. See the [Dependabot options reference](https://docs.github.com/en/code-security/reference/supply-chain-security/dependabot-options-reference).

The [pull-request template](.github/pull_request_template.md), [bug form](.github/ISSUE_TEMPLATE/bug_report.yml), and [feature form](.github/ISSUE_TEMPLATE/feature_request.yml) collect scope and evidence without assigning maintainers or promising support. Ownership rules and a private security-reporting policy require established maintainers and a reporting channel before corresponding configuration is added.

After authorized publication, enable Actions and allow the pinned GitHub-owned actions in repository settings. The default branch must contain the workflow for manual dispatch and `dependabot.yml` for version-update checks. Observe an actual successful hosted run before selecting `portable-core-and-configuration` as a required status check in a branch ruleset or protection rule. Repository files alone do not enable branch protection, security features, or reviewer requirements. If a merge queue is later enabled, add a `merge_group` trigger before requiring this check in that queue.
