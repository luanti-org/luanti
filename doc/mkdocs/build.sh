#!/bin/sh -e

# Split lua_api.md on top level headings
cat ../lua_api.md | csplit -sz -f docs/section - '/^=/-1' '{*}'

cat > mkdocs.yml << EOF
site_name: Luanti API Documentation
theme:
    name: readthedocs
    highlightjs: False
extra_css:
    - css/code_styles.css
    - css/extra.css
markdown_extensions:
    - toc:
        permalink: True
    - pymdownx.superfences
    - pymdownx.highlight:
        css_class: codehilite
    - gfm_admonition
plugins:
    - search:
        separator: '[\s\-\.\(]+'
nav:
- "Home": index.md
EOF

mv docs/section00 docs/index.md

for f in docs/section*
do
	title=$(head -1 $f)
	fname=$(echo $title | tr '[:upper:]' '[:lower:]')
	fname=$(echo $fname | sed 's/ /-/g')
	fname=$(echo $fname | sed "s/'//g").md
	mv $f docs/$fname
	echo "- \"$title\": $fname" >> mkdocs.yml
done

mkdocs build --site-dir ../../public
