## Contributing to Axity

Thank you for your interest in contributing to **Axity**! Contributions of all kinds are welcome, including bug fixes, documentation improvements, tests, tooling, and new language features.

---

### How to Contribute

#### Reporting Bugs or Requesting Features

If you encounter a bug or have a feature request:

* Check existing issues to avoid duplicates
* Open a new issue with a clear title
* Include steps to reproduce, expected behavior, and actual behavior
* Attach logs, errors, or code snippets when applicable

---

### Branching & Workflow

To keep development organized and reviewable, **all contributions must be pushed to a development testing branch**.

Branch naming format:

* `dev-testing-<your-username>-<short-description>`

Examples:

* `dev-testing-john-parser-fix`
* `dev-testing-alice-docs-update`

Workflow:

1. Fork the repository (if needed)
2. Create a new branch from `main` using the naming format above
3. Make your changes on that branch
4. Keep commits small and meaningful
5. Rebase frequently to stay up to date with `main`

---

### Pull Requests

When opening a pull request:

* Target the `main` branch
* Clearly describe what the change does and why it is needed
* Include testing instructions or examples
* Ensure `cargo test` passes before submitting
* Large changes may be split into multiple PRs

Pull requests that do not follow the branching or contribution guidelines may be requested to update or may be closed.

---

### License

By contributing to Axity, you agree that your contributions will be licensed under the project’s existing license.
