#!/usr/bin/env python3
"""Build the data files that FuSION 2.0 ships in share/fusion/data/ (architecture section 11,
"Finding the backend's own files (S1.20)"; prompt log entries 204 and 205; S8.31):

  hsa_to_gene_symbol_map.txt     from NCBI Gene's human gene table
  hsa_path_to_path_name_map.txt  from Reactome's list of pathways, human pathways only
  kegg_pathways.txt              the candidate pathways of FuSION 1's list, as bare ids
  SOURCES.md                     each file's source, when it was taken, the SHA-256 of what was
                                 taken, its licence and the attribution owed

Usage, from anywhere:

  python3 packaging/data/build_data.py [--sources DIR] [--fusion1-list FILE] [--out DIR]

The script downloads NCBI's Homo_sapiens.gene_info.gz and Reactome's ReactomePathways.txt over
HTTPS, reads Reactome's release number, and copies FuSION 1's list. With --sources DIR the three
inputs are kept in DIR, with sources.json recording when each was taken; a later run given the
same DIR uses them as they are and writes the same four files byte for byte.

Nothing from KEGG beyond bare pathway ids is written (S8.31). The licence wording in SOURCES.md
was read from NCBI's, HGNC's and Reactome's sites on 2026-11-18: read it again before shipping a
rebuild.
"""

import argparse
import datetime
import email.utils
import gzip
import hashlib
import json
import re
import shutil
import sys
import tempfile
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GENE_INFO_URL = "https://ftp.ncbi.nlm.nih.gov/gene/DATA/GENE_INFO/Mammalia/Homo_sapiens.gene_info.gz"
REACTOME_URL = "https://reactome.org/download/current/ReactomePathways.txt"
REACTOME_RELEASE_URL = "https://reactome.org/ContentService/data/database/version"
GENE_INFO = "Homo_sapiens.gene_info.gz"
REACTOME = "ReactomePathways.txt"
FUSION1_LIST = "list_of_kegg_xml_files.txt"
RECORD = "sources.json"


def fetch(url, timeout):
    # Reactome's site refuses Python's default user agent, so the request names a generic client.
    return urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "data-build/1.0"}),
                                  timeout=timeout)


def fail(message):
    sys.exit("build_data.py: " + message)


def utc_text(moment):
    return moment.astimezone(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")


def now_utc():
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url, dest):
    """Saves url to dest; returns the server's Last-Modified time, as UTC text, or None."""
    with fetch(url, 300) as response, open(dest, "wb") as out:
        shutil.copyfileobj(response, out)
        modified = response.headers.get("Last-Modified")
    return utc_text(email.utils.parsedate_to_datetime(modified)) if modified else None


def take_sources(folder, fusion1_list):
    """Fills folder with the three inputs and returns the record of when each was taken."""
    record_file = folder / RECORD
    if record_file.exists():
        record = json.loads(record_file.read_text(encoding="utf-8"))
        for name in (GENE_INFO, REACTOME, FUSION1_LIST):
            if not (folder / name).is_file():
                fail(f"{folder / name} is missing, although {record_file} lists it")
        return record
    if not fusion1_list.is_file():
        fail(f"FuSION 1's list {fusion1_list} is missing; name it with --fusion1-list")
    record = {}
    for key, url, name in (("gene_info", GENE_INFO_URL, GENE_INFO), ("reactome", REACTOME_URL, REACTOME)):
        taken = utc_text(now_utc())
        modified = download(url, folder / name)
        record[key] = {"url": url, "taken": taken, "last_modified": modified}
    with fetch(REACTOME_RELEASE_URL, 60) as response:
        release = response.read().decode("utf-8").strip()
    if not release.isdigit():
        fail(f"{REACTOME_RELEASE_URL} answered {release!r}, not a release number")
    record["reactome"]["release"] = release
    shutil.copyfile(fusion1_list, folder / FUSION1_LIST)
    modified = datetime.datetime.fromtimestamp(fusion1_list.stat().st_mtime, datetime.timezone.utc)
    record["fusion1_list"] = {"path": str(fusion1_list), "taken": utc_text(now_utc()),
                              "last_modified": utc_text(modified)}
    record_file.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def symbol_rows(gene_info):
    """Each human gene with its official (HGNC) symbol, and each protein-coding gene that has none
    yet with NCBI's interim symbol, in GeneID order."""
    with gzip.open(gene_info, "rt", encoding="utf-8", newline="\n") as f:
        header = f.readline().rstrip("\n").split("\t")
        column = {name: i for i, name in enumerate(header)}
        for name in ("#tax_id", "GeneID", "Symbol", "type_of_gene", "Symbol_from_nomenclature_authority"):
            if name not in column:
                fail(f"{gene_info} has no column {name}; NCBI changed the table's layout")
        rows = {}
        for number, line in enumerate(f, 2):
            cells = line.rstrip("\n").split("\t")
            if len(cells) != len(header):
                fail(f"{gene_info} line {number} has {len(cells)} columns, not {len(header)}")
            if cells[column["#tax_id"]] != "9606":   # the file also holds two archaic Homo taxa
                continue
            official = cells[column["Symbol_from_nomenclature_authority"]]
            if official != "-":
                symbol = official
            elif cells[column["type_of_gene"]] == "protein-coding":
                symbol = cells[column["Symbol"]]
            else:
                continue
            gene = cells[column["GeneID"]]
            if not re.fullmatch(r"[0-9]+", gene) or int(gene) in rows:
                fail(f"{gene_info} line {number}: GeneID {gene!r} is not a new number")
            if not symbol or symbol == "-" or re.search(r"\s", symbol):
                fail(f"{gene_info} line {number}: GeneID {gene} has the symbol {symbol!r}")
            rows[int(gene)] = symbol
    genes_of = {}
    for gene, symbol in rows.items():
        genes_of.setdefault(symbol, []).append(gene)
    shared = {symbol: genes for symbol, genes in genes_of.items() if len(genes) > 1}
    if shared:   # the map is also read backwards (id_map=symbols), so a symbol must name one gene
        fail("symbols that name more than one gene: " +
             "; ".join(f"{s} ({', '.join(map(str, g))})" for s, g in sorted(shared.items())))
    return sorted(rows.items())


def pathway_rows(reactome):
    """Each human pathway as FuSION 1's maps wrote it: pathway_<n> for Reactome's R-HSA-<n>, and the
    name with its spaces written as '_' (S1.19), in order of n."""
    rows = {}
    with open(reactome, encoding="utf-8", newline="\n") as f:
        for number, line in enumerate(f, 1):
            cells = line.rstrip("\n").split("\t")
            if len(cells) != 3:
                fail(f"{reactome} line {number} has {len(cells)} columns, not 3")
            stable_id, name, species = cells
            if species != "Homo sapiens":
                continue
            match = re.fullmatch(r"R-HSA-([0-9]+)", stable_id)
            if not match or int(match.group(1)) in rows:
                fail(f"{reactome} line {number}: {stable_id!r} is not a new human stable id")
            label = "_".join(name.split())
            if not label:
                fail(f"{reactome} line {number}: {stable_id} has no name")
            rows[int(match.group(1))] = label
    return sorted(rows.items())


def kegg_ids(listing):
    """The pathway id of each KGML file in FuSION 1's list, in the list's order."""
    ids = []
    with open(listing, encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            path = line.strip()
            if not path:
                continue
            match = re.fullmatch(r"(hsa[0-9]{5})\.xml", path.replace("\\", "/").rsplit("/", 1)[-1])
            if not match or match.group(1) in ids:
                fail(f"{listing} line {number}: {path!r} does not name a new hsaNNNNN.xml file")
            ids.append(match.group(1))
    return ids


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(line + "\n" for line in lines)


def sources_md(record, digests, counts):
    gene, reactome, listing = record["gene_info"], record["reactome"], record["fusion1_list"]
    day = {key: record[key]["taken"][:10] for key in record}

    def modified(entry):
        return f"; the copy taken was last modified {entry['last_modified']}" if entry.get("last_modified") else ""

    return f"""# Sources of the shipped data

FuSION 2.0 ships the files of this folder in its home, as `$FUSION_HOME/data/` (architecture section 11; S8.31). `packaging/data/build_data.py` wrote all four, this one included, and rebuilds them. Times are UTC.

| File | Source | Taken | Licence |
|---|---|---|---|
| `hsa_to_gene_symbol_map.txt` | NCBI Gene | {day['gene_info']} | no restrictions (NCBI and HGNC) |
| `hsa_path_to_path_name_map.txt` | Reactome release {reactome['release']} | {day['reactome']} | CC0 1.0 |
| `kegg_pathways.txt` | FuSION 1's candidate list, ids only | {day['fusion1_list']} | MIT (FuSION 2.0) |

## `hsa_to_gene_symbol_map.txt`

- **Source:** NCBI Gene's human gene table, `{GENE_INFO}`.
- **URL:** <{gene['url']}>
- **Taken:** {gene['taken']}{modified(gene)}.
- **SHA-256 of the download:** `{digests['gene_info']}`
- **Contents:** {counts['symbols']:,} lines of `hsa<GeneID><TAB><symbol>`, in GeneID order. Each human gene (tax_id 9606) that has an official symbol, the HGNC symbol in the table's `Symbol_from_nomenclature_authority` column, has a line with that symbol. Each protein-coding gene that has no official symbol yet has a line with NCBI's interim symbol, from the `Symbol` column (such as `LOC102724594`). The other genes without an official symbol are left out: biological regions, and ncRNA genes, pseudogenes and other loci that HGNC has not named. No symbol names two genes, so the map can also be read backwards, as `id_map=symbols` reads it.
- **Licence:** NCBI's Website and Data Usage Policies (<https://www.ncbi.nlm.nih.gov/home/about/policies/>, Molecular Data Usage): "NCBI itself places no restrictions on the use or distribution of the data contained therein." The official symbols are HGNC's, whose data release policy (<https://www.genenames.org/about/>) says: "No restrictions are imposed on access to, or use of, the data provided by the HGNC".
- **Attribution owed:** none is required. NCBI asks that the National Library of Medicine be given appropriate acknowledgment, and the HGNC asks its users to reference the "HUGO Gene Nomenclature Committee at the University of Cambridge" and its website. FuSION gives both: gene symbols from NCBI Gene, National Center for Biotechnology Information, U.S. National Library of Medicine (<https://www.ncbi.nlm.nih.gov/gene/>), with the official symbols of the HUGO Gene Nomenclature Committee at the University of Cambridge (HGNC, <https://www.genenames.org/>, RRID:SCR_002827).

## `hsa_path_to_path_name_map.txt`

- **Source:** Reactome's list of pathways, `{REACTOME}`, Reactome release {reactome['release']}.
- **URL:** <{reactome['url']}>
- **Taken:** {reactome['taken']}{modified(reactome)}.
- **SHA-256 of the download:** `{digests['reactome']}`
- **Contents:** {counts['pathways']:,} lines of `pathway_<n><TAB><name>`, one for each human pathway (stable id `R-HSA-<n>`, species Homo sapiens), in order of `<n>`. The id is the form FuSION 1's maps gave Reactome's stable ids, and the name is Reactome's, with its spaces written as `_`, the convention of FuSION 1's maps (S1.19). The file holds no KEGG pathway name: FuSION reads those from the title of each KGML in the user's own folder.
- **Licence:** Reactome's License Agreement (<https://reactome.org/license>), section 1(c): "All data in the Reactome database and files derived from that data are licensed under the Creative Commons Public Domain Dedication (CC0)." It adds: "Attribution is encouraged but not required." Only Reactome's pathway illustrations, icons, art and branding are under CC BY 4.0 (its section 1(a)), and none of them ships.
- **Attribution owed:** none is required. FuSION gives it in the form Reactome asks for downloaded files: "{REACTOME}", Reactome, release {reactome['release']}, <https://reactome.org/download-data/> (accessed {day['reactome']}). Changes made: human pathways only; ids written as `pathway_<n>` for `R-HSA-<n>`; spaces in names written as `_`. Reactome's latest paper: Ragueneau E, Gong C, Sinquin P, et al. The Reactome Knowledgebase 2026. Nucleic Acids Res. 2025. doi:10.1093/nar/gkaf1223.

## `kegg_pathways.txt`

- **Source:** FuSION 1's list of candidate KGML files, `auxilliary/{FUSION1_LIST}` in FuSION 1's working tree.
- **URL:** none; the file is FuSION 1's own.
- **Taken:** {listing['taken']}; the copy taken was last modified {listing['last_modified']}.
- **SHA-256 of FuSION 1's list:** `{digests['fusion1_list']}`
- **Contents:** {counts['kegg']:,} lines, one KEGG pathway id each (such as `hsa04010`), in the list's order: the name of each listed file without `.xml`. FuSION resolves them against the user's own KGML folder (02.C12). No KGML ships (S8.31), and the ids carry no KEGG content.
- **Licence:** MIT, as the rest of FuSION 2.0.
- **Attribution owed:** none.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sources", type=Path,
                        help="keep the three inputs here, or reuse them if an earlier run kept them here")
    parser.add_argument("--fusion1-list", type=Path,
                        default=REPO.parent / "FuSION" / "auxilliary" / FUSION1_LIST,
                        help="FuSION 1's list of candidate KGML files (default: %(default)s)")
    parser.add_argument("--out", type=Path, default=REPO / "share" / "fusion" / "data",
                        help="where the four files go (default: %(default)s)")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as scratch:
        folder = args.sources if args.sources else Path(scratch)
        folder.mkdir(parents=True, exist_ok=True)
        record = take_sources(folder, args.fusion1_list.resolve())
        symbols = symbol_rows(folder / GENE_INFO)
        pathways = pathway_rows(folder / REACTOME)
        kegg = kegg_ids(folder / FUSION1_LIST)
        digests = {"gene_info": sha256(folder / GENE_INFO), "reactome": sha256(folder / REACTOME),
                   "fusion1_list": sha256(folder / FUSION1_LIST)}

    args.out.mkdir(parents=True, exist_ok=True)
    write_lines(args.out / "hsa_to_gene_symbol_map.txt", (f"hsa{gene}\t{symbol}" for gene, symbol in symbols))
    write_lines(args.out / "hsa_path_to_path_name_map.txt", (f"pathway_{n}\t{name}" for n, name in pathways))
    write_lines(args.out / "kegg_pathways.txt", kegg)
    counts = {"symbols": len(symbols), "pathways": len(pathways), "kegg": len(kegg)}
    with open(args.out / "SOURCES.md", "w", encoding="utf-8", newline="\n") as f:
        f.write(sources_md(record, digests, counts))
    for name, count in (("hsa_to_gene_symbol_map.txt", len(symbols)),
                        ("hsa_path_to_path_name_map.txt", len(pathways)), ("kegg_pathways.txt", len(kegg))):
        print(f"{args.out / name}: {count} lines")
    print(f"{args.out / 'SOURCES.md'}: written")


if __name__ == "__main__":
    main()
