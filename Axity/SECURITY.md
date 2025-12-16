## Security Policy

The Axity project takes security seriously. We appreciate responsible disclosure of security vulnerabilities and will work to address them as quickly as possible.

---

### Reporting a Vulnerability

If you discover a security issue, **do not open a public issue**.

Instead, report it privately by contacting the project maintainers.

Include the following information if possible:

* A clear description of the vulnerability
* Affected components or versions
* Steps to reproduce the issue
* Proof-of-concept code or examples
* Potential impact

You will receive an acknowledgment within 72 hours.

---

### Supported Versions

Security updates are provided for:

* The `main` branch
* The most recent released versions (if applicable)

Older versions may not receive fixes.

---

### Responsible Disclosure

Please allow time for the vulnerability to be investigated and fixed before any public disclosure. Once resolved, relevant information may be published in release notes or advisories.

---

### Contributor Security Responsibilities

Contributors should avoid introducing:

* Hard-coded secrets or credentials
* Unsafe or undefined behavior without justification
* Unsanitized user input in compiler or runtime components
* Insecure default configurations

If you are unsure whether a change impacts security, please ask before submitting a pull request.
