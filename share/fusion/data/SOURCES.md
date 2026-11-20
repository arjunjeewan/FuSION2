# Sources of the shipped data

FuSION 2.0 ships the files of this folder in its home, as `$FUSION_HOME/data/` (architecture section 11; S8.31). `packaging/data/build_data.py` wrote all four, this one included, and rebuilds them. Times are UTC.

| File | Source | Taken | Licence |
|---|---|---|---|
| `hsa_to_gene_symbol_map.txt` | NCBI Gene | 2026-11-18 | no restrictions (NCBI and HGNC) |
| `hsa_path_to_path_name_map.txt` | Reactome release 98 | 2026-11-18 | CC0 1.0 |
| `kegg_pathways.txt` | FuSION 1's candidate list, ids only | 2026-11-18 | MIT (FuSION 2.0) |

## `hsa_to_gene_symbol_map.txt`

- **Source:** NCBI Gene's human gene table, `Homo_sapiens.gene_info.gz`.
- **URL:** <https://ftp.ncbi.nlm.nih.gov/gene/DATA/GENE_INFO/Mammalia/Homo_sapiens.gene_info.gz>
- **Taken:** 2026-11-18 19:58 UTC; the copy taken was last modified 2026-11-18 11:15 UTC.
- **SHA-256 of the download:** `d76d27363c354557d5eb1799875a502e9d2b7a1491f6de3d2947ac8be871b4b6`
- **Contents:** 46,530 lines of `hsa<GeneID><TAB><symbol>`, in GeneID order. Each human gene (tax_id 9606) that has an official symbol, the HGNC symbol in the table's `Symbol_from_nomenclature_authority` column, has a line with that symbol. Each protein-coding gene that has no official symbol yet has a line with NCBI's interim symbol, from the `Symbol` column (such as `LOC102724594`). The other genes without an official symbol are left out: biological regions, and ncRNA genes, pseudogenes and other loci that HGNC has not named. No symbol names two genes, so the map can also be read backwards, as `id_map=symbols` reads it.
- **Licence:** NCBI's Website and Data Usage Policies (<https://www.ncbi.nlm.nih.gov/home/about/policies/>, Molecular Data Usage): "NCBI itself places no restrictions on the use or distribution of the data contained therein." The official symbols are HGNC's, whose data release policy (<https://www.genenames.org/about/>) says: "No restrictions are imposed on access to, or use of, the data provided by the HGNC".
- **Attribution owed:** none is required. NCBI asks that the National Library of Medicine be given appropriate acknowledgment, and the HGNC asks its users to reference the "HUGO Gene Nomenclature Committee at the University of Cambridge" and its website. FuSION gives both: gene symbols from NCBI Gene, National Center for Biotechnology Information, U.S. National Library of Medicine (<https://www.ncbi.nlm.nih.gov/gene/>), with the official symbols of the HUGO Gene Nomenclature Committee at the University of Cambridge (HGNC, <https://www.genenames.org/>, RRID:SCR_002827).

## `hsa_path_to_path_name_map.txt`

- **Source:** Reactome's list of pathways, `ReactomePathways.txt`, Reactome release 98.
- **URL:** <https://reactome.org/download/current/ReactomePathways.txt>
- **Taken:** 2026-11-18 19:58 UTC; the copy taken was last modified 2026-10-18 04:47 UTC.
- **SHA-256 of the download:** `b8103a50d625ffb6acd18cddffcce0ceb0fdf36b19392d104c32bb1fcc7e2f9f`
- **Contents:** 2,889 lines of `pathway_<n><TAB><name>`, one for each human pathway (stable id `R-HSA-<n>`, species Homo sapiens), in order of `<n>`. The id is the form FuSION 1's maps gave Reactome's stable ids, and the name is Reactome's, with its spaces written as `_`, the convention of FuSION 1's maps (S1.19). The file holds no KEGG pathway name: FuSION reads those from the title of each KGML in the user's own folder.
- **Licence:** Reactome's License Agreement (<https://reactome.org/license>), section 1(c): "All data in the Reactome database and files derived from that data are licensed under the Creative Commons Public Domain Dedication (CC0)." It adds: "Attribution is encouraged but not required." Only Reactome's pathway illustrations, icons, art and branding are under CC BY 4.0 (its section 1(a)), and none of them ships.
- **Attribution owed:** none is required. FuSION gives it in the form Reactome asks for downloaded files: "ReactomePathways.txt", Reactome, release 98, <https://reactome.org/download-data/> (accessed 2026-11-18). Changes made: human pathways only; ids written as `pathway_<n>` for `R-HSA-<n>`; spaces in names written as `_`. Reactome's latest paper: Ragueneau E, Gong C, Sinquin P, et al. The Reactome Knowledgebase 2026. Nucleic Acids Res. 2025. doi:10.1093/nar/gkaf1223.

## `kegg_pathways.txt`

- **Source:** FuSION 1's list of candidate KGML files, `auxilliary/list_of_kegg_xml_files.txt` in FuSION 1's working tree.
- **URL:** none; the file is FuSION 1's own.
- **Taken:** 2026-11-18 19:58 UTC; the copy taken was last modified 2026-09-28 12:25 UTC.
- **SHA-256 of FuSION 1's list:** `86fc603f19d96981c688e8356212b45a880c888abb162f2e6abe16ee10156bb8`
- **Contents:** 167 lines, one KEGG pathway id each (such as `hsa04010`), in the list's order: the name of each listed file without `.xml`. FuSION resolves them against the user's own KGML folder (02.C12). No KGML ships (S8.31), and the ids carry no KEGG content.
- **Licence:** MIT, as the rest of FuSION 2.0.
- **Attribution owed:** none.
