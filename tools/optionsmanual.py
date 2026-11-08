"""The manual's settings pages (W616): docs/manual/15-settings-panels.md has one section for each settings panel, the group's title, its
sections and each row's label and help line, generated from the same option table as the panels (the schema `xppautX --options-schema`
prints is built from it), so the manual, `help` and the page say one thing.

    uv run --locked python tools/optionsmanual.py --check SCHEMA.json    fail when the page differs
    uv run --locked python tools/optionsmanual.py --update SCHEMA.json   rewrite it

tools/optionschema.sh runs it with the schema it has just printed.
"""
import json
import os
import sys

PAGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'manual', '15-settings-panels.md')
INTRO = """# The settings panels

Every group of settings has one panel, titled by the group's name, and its rows are in the order of the option table
(`core/model_options.cpp`), the one source of a setting's key, label, choices, default, limits and help line. A dotted key
(`step.min`) is a titled section inside the panel. The same keys are the `options` block of a `.odex`, the `help` of the program and
the manual below. Each panel opens from the menu entry of its task: Integration from Run > Run settings, Poincare from Run > Poincare map,
Sweep from Run > Over a range > Run over a range (beside its Run button), Equilibria from Analysis > Equilibria, Analysis inside the
histogram and spectrum commands' forms, Continuation from autoX > Numerics, Diagram from autoX > Axes (it changes the open view; the model's
values only start a new one), Plot from Plot > Axes > Axes options (it changes the active graph), Export inside the picture export
commands' forms and Output from File > Output, where each path has a Choose file button. The thread count and the log are File > Preferences. Values > Numerics shows the Integration rows beside the plot, each change applying at once.
Apply sets the changed rows together, all or nothing, as one undo step. The values of a panel come from the core when the running command
ends; the interface group (sliders, buttons, animation) has its own editors.
"""


def label(name):
    """core/options_panels.cpp words_of: the key as words, colour spelt as the page spells it"""
    spaced = name.replace('_', ' ').replace('olor', 'olour')
    return spaced[:1].upper() + spaced[1:]


def page(schema):
    out = [INTRO]
    for group, body in schema['properties'].items():
        props = body.get('properties')
        if not props:
            continue
        out.append(f"\n## {label(group)}\n")
        section = None
        for key, row in props.items():
            if 'description' not in row:
                continue
            head, _, leaf = key.rpartition('.')
            if head != section and head:
                out.append(f"\n### {label(head)}\n")
            section = head
            out.append(f"- **{label(leaf)}** (`{key}`): {row['description']}")
    return '\n'.join(out) + '\n'


def main():
    mode, path = sys.argv[1], sys.argv[2]
    with open(path, encoding='utf-8') as f:
        text = page(json.load(f))
    if mode == '--update':
        with open(PAGE, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        print('optionsmanual: wrote docs/manual/15-settings-panels.md')
        return 0
    with open(PAGE, encoding='utf-8', newline='') as f:
        have = f.read().replace('\r\n', '\n')
    if have == text:
        print('optionsmanual ok: docs/manual/15-settings-panels.md matches the table')
        return 0
    print('optionsmanual: docs/manual/15-settings-panels.md differs from the table (tools/optionschema.sh --update)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
