# Continuous Integration

The files in this directory are used for continuous integration testing.
`ci/install` installs the prerequisite packages (run as root on a Debian
derivative), and `ci/test` runs the tests.

Tests are run automatically via GitHub Actions workflows using these
scripts and the configuration in the `.github/workflows` directory.
