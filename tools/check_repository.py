"""Validate published repository content; intentionally local PDFs are never fetched."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit
from urllib.request import urlopen

import jsonschema
from markdown_it import MarkdownIt
import yaml


ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = "docs/proflame2/sources/"
LOCAL_PDFS = {ARCHIVE + name for name in (
    "marquis-proflame2.pdf", "montigo-wifi-xg0681.pdf", "sit-ifc-ffrt10-2018.pdf",
    "sit-system-9955116-02.pdf", "sit-transmitter-ce-9957120-00.pdf",
    "sit-wall-transmitter.pdf", "ti-cc1101.pdf",
)}


class UniqueLoader(yaml.SafeLoader):
    """Reject duplicate configuration keys before schema validation."""

    def construct_mapping(self, node, deep=False):
        result = {}
        for key_node, value_node in node.value:
            key = self.construct_object(key_node, deep=deep)
            if key in result:
                raise ValueError(f"Duplicate YAML key: {key}")
            result[key] = self.construct_object(value_node, deep=deep)
        return result


def repository_files():
    """Include local proposed additions while excluding generated and private files."""
    output = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
                                     cwd=ROOT)
    return {name for name in output.decode("utf-8").split("\0") if name}


def checked_schema(name):
    """Cache a SHA-256-verified schema from an immutable SchemaStore revision."""
    specification = json.loads((ROOT / "tools/ci-schemas.json").read_text())[name]
    target = ROOT / "build/ci-schemas" / (name + ".json")
    if target.exists():
        data = target.read_bytes()
    else:
        with urlopen(specification["url"], timeout=30) as response:
            data = response.read()
        target.parent.mkdir(parents=True, exist_ok=True)
    if hashlib.sha256(data).hexdigest() != specification["sha256"]:
        raise ValueError(f"Schema integrity failure: {name}")
    target.write_bytes(data)
    return json.loads(data)


def anchors(tokens):
    """Collect GitHub-style heading slugs, including repeated headings."""
    result, counts = set(), {}
    for index, token in enumerate(tokens):
        if token.type == "heading_open":
            inline = tokens[index + 1]
            text = "".join(child.content for child in inline.children or []
                           if child.type in ("text", "code_inline"))
            slug = re.sub(r"[^\w\- ]", "", text.lower()).replace(" ", "-")
            number = counts.get(slug, 0)
            counts[slug] = number + 1
            result.add(slug if number == 0 else f"{slug}-{number}")
    return result


def check_links(files):
    """Check maintained Markdown links offline against publishable files and anchors."""
    parser = MarkdownIt()
    documents = {name: parser.parse((ROOT / name).read_text(encoding="utf-8"))
                 for name in files if name.endswith(".md")}
    count = 0
    for name, tokens in documents.items():
        for token in tokens:
            for child in token.children or []:
                target = child.attrGet("href") if child.type == "link_open" else child.attrGet("src")
                if not target:
                    continue
                link = urlsplit(target)
                if link.scheme or link.netloc:
                    continue
                destination = ((ROOT / name).parent / unquote(link.path)).resolve() if link.path else ROOT / name
                relative = destination.relative_to(ROOT).as_posix()
                if relative in LOCAL_PDFS:
                    continue
                if relative not in files and not any(item.startswith(relative + "/") for item in files):
                    raise ValueError(f"Unpublished link target: {name}: {target}")
                if link.fragment and relative in documents and unquote(link.fragment) not in anchors(documents[relative]):
                    raise ValueError(f"Missing heading: {name}: {target}")
                count += 1
    print(f"Validated {count} local links and anchors")


def check_archive(files):
    """Enforce PDF exclusion and verify only published archive bytes against provenance."""
    tracked = set(subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0"))
    if tracked & LOCAL_PDFS:
        raise ValueError("A local-only manufacturer PDF is tracked")
    for name in LOCAL_PDFS:
        subprocess.run(["git", "check-ignore", "--no-index", "--quiet", name], cwd=ROOT, check=True)
    count = 0
    for entry in json.loads((ROOT / ARCHIVE / "manifest.json").read_text()):
        name = ARCHIVE + entry["file"]
        if "sha256" not in entry or name in LOCAL_PDFS:
            continue
        if name not in files or hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Missing or altered retained reference: {name}")
        count += 1
    print(f"Validated {count} published reference hashes; seven PDFs remain local-only")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actionlint", default="actionlint")
    args = parser.parse_args()
    files = repository_files()
    subprocess.run([args.actionlint, ".github/workflows/host-checks.yml"], cwd=ROOT, check=True)
    for path in sorted(files):
        if path.startswith("tools/") and path.endswith(".sh"):
            subprocess.run(["bash", "-n", path], cwd=ROOT, check=True)
    configurations = {".github/dependabot.yml": "dependabot-2.0"}
    configurations.update({name: "github-issue-forms" for name in files
                           if name.startswith(".github/ISSUE_TEMPLATE/") and name.endswith(".yml")})
    for name, schema in configurations.items():
        data = yaml.load((ROOT / name).read_text(), Loader=UniqueLoader)
        jsonschema.validate(data, checked_schema(schema))
        if schema == "github-issue-forms":
            ids = [field["id"] for field in data["body"] if "id" in field]
            if len(ids) != len(set(ids)):
                raise ValueError(f"Duplicate issue-form field: {name}")
    check_archive(files)
    check_links(files)
    print("Repository checks passed")


if __name__ == "__main__":
    main()
