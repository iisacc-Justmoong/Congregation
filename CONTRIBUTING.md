# Contributing to Congregation

Congregation welcomes reproducible bug reports, Windows hardware testing, documentation improvements, and focused code contributions.

## Start with a public discussion or issue

- Use [GitHub Discussions](https://github.com/iisacc-Justmoong/Congregation/discussions) for general feedback, design ideas, and questions.
- Use [GitHub Issues](https://github.com/iisacc-Justmoong/Congregation/issues) for reproducible defects or bounded implementation work.
- The [Windows 10/11 testing issue](https://github.com/iisacc-Justmoong/Congregation/issues/18) lists useful first validation tasks.

Include the operating-system version, hardware or input device, exact reproduction steps, expected result, and actual result. Remove personal information from screenshots and logs.

## Build and test

Keep every generated build file in the repository-local `build/` directory:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the complete test suite before submitting a pull request. New behavior should normally begin with or include an automated regression test. UI prototypes and exploratory work may establish the implementation first, but must add appropriate tests and documentation before review.

## Project rules

- Follow the Qt and KDE coding guidelines, the repository `.clang-format`, four-space indentation, and Allman braces.
- QML changes must use the `.local/SDK/LVRS/` framework and keep one root component per file.
- Keep modules replaceable, avoid circular dependencies, and do not make lower-level modules depend on higher-level modules.
- Evaluate maintained, suitably licensed external libraries before implementing non-domain infrastructure locally.
- Update relevant documentation and tests with every source change.
- Do not commit generated binaries, credentials, private keys, certificate files, or signing tokens.

## Windows release safety

Unsigned MSI, ZIP, MSIX, and self-signed SignPath trial outputs are development-only artifacts. Any Necessary workflow input created before service approval is development-only as well. They must not be attached to a public release or presented as trusted downloads. Never commit or print `NECESSARY_SIGN_TOKEN`; it belongs only in the GitHub Actions repository-secret store after the provider approves Congregation. A website MSI is publishable only after its outer MSI and nested `Congregation.exe` both pass the repository's trusted Authenticode, expected-Publisher, same-certificate, and RFC 3161 timestamp verification gates.
