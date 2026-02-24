# Luanti API Documentation Generator

## Building HTML files from the API reference

### Setup

Use `bash`. Windows users may try "Git Bash".

```sh
# New virtual environment
python3 -m venv venv
# Note: This changes $PATH of the current shell
source venv/bin/activate
pip install -r requirements.txt
```

### Building

```sh
# Such that "mkdocs" can be found within the venv
source venv/bin/activate
./build.sh
```

See also: https://www.mkdocs.org/user-guide/cli/

Output directories:

* Markdown files: `doc/mkdocs/docs/`
* HTML (for browsers): `public/index.html`
