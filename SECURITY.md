# Security Policy

## Supported Versions

INN is developed on the `main` branch, which tracks the next major version.
The stable branch receives bug fixes, security fixes and backports of some
enhancements.  The most recent old stable branch only receives security fixes,
and so long as the first minor version of the stable branch has been released.

Security fixes are therefore provided for:

| Branch | Status | First release | Last release (EOL) | Security support |
| ------ | -------| ------------- | ------------------ | ---------------- |
| main   | Next major version | [Snapshots](https://downloads.isc.org/isc/inn/snapshots/) (2.8.0) | not announced | :white_check_mark: |
| 2.7 | Stable version     | 2022-07-10 (2.7.0) | not announced      | :white_check_mark: (until the 2.8.1 release) |
| 2.6 | Old stable version | 2015-09-12 (2.6.0) | 2022-02-18 (2.6.5) | :x: |
| 2.5 | Old stable version | 2009-05-21 (2.5.0) | 2015-05-23 (2.5.5) | :x: |
| 2.4 | Old stable version | 2003-05-06 (2.4.0) | 2009-02-27 (2.4.6) | :x: |
| 2.3 | Old stable version | 2000-08-20 (2.3.0) | 2003-03-08 (2.3.5) | :x: |
| 2.2 | Old stable version | 1999-01-21 (2.2)   | 2000-07-18 (2.2.3) | :x: |
| 2.1 | Old stable version | 1998-07-24 (2.1)   | 1998-07-24 (2.1)   | :x: |
| 2.0 | Old stable version | 1998-06-08 (2.0)   | 1998-06-08 (2.0)   | :x: |

If you are running a no longer supported branch, please upgrade to the stable
version before reporting a suspected vulnerability, since it may already have
been fixed.

## Reporting a Vulnerability

Please do **NOT** report security vulnerabilities through GitHub issues or
pull requests, the `inn-workers` mailing-list or the `news.software.nntp`
newsgroup because they are all **public**.

Instead, please report them **privately** using the [GitHub Security
Advisories](https://github.com/InterNetNews/inn/security/advisories/) of the
INN project.  This creates a private advisory that only the INN maintainers
(Russ Allbery and Julien Élie) can see until it is resolved and you both
agree to disclose it.

When reporting, please include as much of the following as you can:
- a description of the vulnerability and its potential impact (for instance
  remote code execution, control message handling, buffer overrun, SQL
  injection, authentication bypass, denial of service);
- the affected component(s) and version(s) with the commit hash which
  introduced the vulnerability;
- clear step-by-step instructions to reproduce the issue, or a minimal proof
  of concept, if you have one;
- a patch, if you have one;
- your assessment of severity and any mitigating factors (for instance
  whether exploitation requires a trusted peer feed or an authenticated reader
  connection).

Please do not disclose the issue publicly before a fix is released.

If you have multiple security issues to report, please create a separate report for each vulnerability.

## What to Expect

- We, the INN maintainers, will acknowledge your report within a few days.
- We will investigate and keep you updated on progress toward a fix.
- Once a fix is ready, we will coordinate a release and a public disclosure
  date with you.
- Credit will be given to you in the release notes and the advisory, unless
  you prefer to remain anonymous.

## Scope

This policy covers the source code in this repository.  Issues in third-party
software that merely integrates with INN should be reported to their own
projects.
