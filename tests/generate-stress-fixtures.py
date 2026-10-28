"""Generate deterministic synthetic Office loads; outputs stay in ignored artifacts.

No user documents, external links, macros or credentials are used. Dataset size
means real cells, formulas, pages, tables and images, not ZIP padding. Re-run with
the same profile to reuse these scenarios in future release acceptance.
"""
import argparse
import hashlib
import io
import json
import random
from pathlib import Path
from xml.sax.saxutils import escape
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
NS = 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'
REL = 'http://schemas.openxmlformats.org/officeDocument/2006/relationships'


def excel(path, rows, cols, sheets, seed):
    rng = random.Random(seed)
    with ZipFile(path, 'w', ZIP_DEFLATED, compresslevel=1) as z:
        z.writestr('[Content_Types].xml', '<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>' + ''.join(f'<Override PartName="/xl/worksheets/sheet{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' for i in range(1, sheets + 1)) + '</Types>')
        z.writestr('_rels/.rels', f'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="{REL}/officeDocument" Target="xl/workbook.xml"/></Relationships>')
        z.writestr('xl/workbook.xml', f'<workbook xmlns="{NS}" xmlns:r="{REL}"><bookViews><workbookView activeTab="0"/></bookViews><sheets>' + ''.join(f'<sheet name="{path.stem[:18]}_{i}" sheetId="{i}" r:id="rId{i}"/>' for i in range(1, sheets + 1)) + '</sheets><calcPr calcId="191029" fullCalcOnLoad="1"/></workbook>')
        z.writestr('xl/_rels/workbook.xml.rels', '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' + ''.join(f'<Relationship Id="rId{i}" Type="{REL}/worksheet" Target="worksheets/sheet{i}.xml"/>' for i in range(1, sheets + 1)) + '</Relationships>')
        for sheet in range(1, sheets + 1):
            with z.open(f'xl/worksheets/sheet{sheet}.xml', 'w') as raw:
                out = io.TextIOWrapper(raw, encoding='utf-8', write_through=True)
                out.write(f'<worksheet xmlns="{NS}"><dimension ref="A1:{chr(64+cols)}{rows}"/><sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/><selection pane="bottomLeft" activeCell="A1" sqref="A1"/></sheetView></sheetViews><cols><col min="1" max="{cols}" width="22" customWidth="1"/></cols><sheetData>')
                for row in range(1, rows + 1):
                    cells = []
                    for col in range(1, cols + 1):
                        address = f'{chr(64+col)}{row}'
                        if row == 1:
                            value = f'{path.stem}|S{sheet}|COL{col}|SYNTHETIC'
                            cells.append(f'<c r="{address}" t="inlineStr"><is><t>{value}</t></is></c>')
                        elif col % 5 == 0:
                            cells.append(f'<c r="{address}"><f>A{row}+B{row}*0.125</f><v>0</v></c>')
                        elif col % 3 == 0:
                            token = f'{rng.getrandbits(96):024x}'
                            value = f'{path.stem}-业务记录-{row}-{token}'
                            cells.append(f'<c r="{address}" t="inlineStr"><is><t>{value}</t></is></c>')
                        else:
                            cells.append(f'<c r="{address}"><v>{rng.randrange(10000000,999999999)}</v></c>')
                    out.write(f'<row r="{row}">' + ''.join(cells) + '</row>')
                out.write(f'</sheetData><autoFilter ref="A1:{chr(64+cols)}{rows}"/></worksheet>')
                out.flush()
                out.detach()
    return dict(rowsPerSheet=rows, columns=cols, sheets=sheets,
                cells=rows*cols*sheets, formulas=(rows-1)*(cols//5)*sheets,
                seed=seed, identity=f'{path.stem}|S1|COL1|SYNTHETIC')


def word(path, pages, images, seed):
    from PIL import Image
    rng = random.Random(seed)
    ns = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
    with ZipFile(ROOT/'tests/fixtures/word-demo.docx') as src, ZipFile(path, 'w', ZIP_DEFLATED, compresslevel=1) as z:
        for item in src.infolist():
            if item.filename not in ('word/document.xml', '[Content_Types].xml', 'word/_rels/document.xml.rels'):
                z.writestr(item, src.read(item.filename))
        types = src.read('[Content_Types].xml').decode('utf-8').replace('</Types>', '<Default Extension="jpg" ContentType="image/jpeg"/></Types>')
        z.writestr('[Content_Types].xml', types)
        rels = src.read('word/_rels/document.xml.rels').decode('utf-8')
        rels = rels.replace('</Relationships>', ''.join(f'<Relationship Id="stressImage{i}" Type="{REL}/image" Target="media/stress-{i}.jpg"/>' for i in range(images))+'</Relationships>')
        z.writestr('word/_rels/document.xml.rels', rels)
        chunks = [f'<w:document xmlns:w="{ns}" xmlns:r="{REL}" xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture"><w:body>']
        for page in range(pages):
            marker = f'{path.stem}|PAGE {page+1}|SYNTHETIC'
            chunks.append(f'<w:p><w:r><w:t>{marker}</w:t></w:r></w:p>')
            for paragraph in range(12):
                text = (f'合成压力测试 第{page+1}页 第{paragraph+1}段。' + f'{rng.getrandbits(128):032x} ')*8
                chunks.append(f'<w:p><w:r><w:t>{escape(text)}</w:t></w:r></w:p>')
            chunks.append('<w:tbl><w:tblPr><w:tblW w:w="9000" w:type="dxa"/></w:tblPr><w:tblGrid>'+('<w:gridCol w:w="3000"/>'*3)+'</w:tblGrid>'+''.join('<w:tr>'+''.join(f'<w:tc><w:tcPr><w:tcW w:w="3000" w:type="dxa"/></w:tcPr><w:p><w:r><w:t>{page}-{r}-{c}</w:t></w:r></w:p></w:tc>' for c in range(3))+'</w:tr>' for r in range(6))+'</w:tbl>')
            if page < images:
                data = rng.randbytes(1200*800*3)
                image = Image.frombytes('RGB', (1200,800), data)
                stream = io.BytesIO(); image.save(stream, 'JPEG', quality=85)
                z.writestr(f'word/media/stress-{page}.jpg', stream.getvalue())
                chunks.append(f'<w:p><w:r><w:drawing><wp:inline><wp:extent cx="4500000" cy="3000000"/><wp:docPr id="{page+1}" name="Synthetic {page}"/><a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:pic><pic:nvPicPr><pic:cNvPr id="{page+1}" name="Synthetic"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed="stressImage{page}"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="4500000" cy="3000000"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>')
            if page < pages-1: chunks.append('<w:p><w:r><w:br w:type="page"/></w:r></w:p>')
        chunks.append('<w:sectPr><w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1000" w:right="1000" w:bottom="1000" w:left="1000"/></w:sectPr></w:body></w:document>')
        z.writestr('word/document.xml', ''.join(chunks))
    return dict(pageSections=pages, tables=pages, images=images, seed=seed,
                identity=f'{path.stem}|PAGE 1|SYNTHETIC')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/stress-fixtures-v1')
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    records = []
    specs = [('Excel-Medium-A.xlsx', excel, (12000,12,3,101)),
             ('Excel-Large-A.xlsx', excel, (50000,20,3,201)),
             ('Excel-Large-B.xlsx', excel, (60000,20,3,202)),
             ('Word-Long-A.docx', word, (250,40,301)),
             ('Word-Long-B.docx', word, (400,60,302))]
    for name, generator, params in specs:
        path = args.output/name
        print('Generating '+name, flush=True)
        metadata = generator(path, *params)
        metadata.update(file=name, bytes=path.stat().st_size,
                        sha256=hashlib.file_digest(path.open('rb'), 'sha256').hexdigest())
        with ZipFile(path) as z: assert z.testzip() is None
        records.append(metadata)
        (args.output/'manifest.json').write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(metadata, ensure_ascii=False), flush=True)


if __name__ == '__main__': main()
