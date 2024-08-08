import cbor2
import json, os, sys
import requests

URL = 'https://raw.githubusercontent.com/json-iterator/test-data/master/large-file.json'
JSON_F = '/tmp/big.json'
CBOR_F = '/tmp/big.cbor'

if not os.path.exists(JSON_F):
    print(' - Downloading json test file:', URL, '->', JSON_F)
    with open(JSON_F, 'w') as fp:
        r = requests.get(URL, allow_redirects=True)
        fp.write(r.content.decode('utf-8'))

if not os.path.exists(CBOR_F):
    print(' - Converting to cbor:', JSON_F, '->', CBOR_F)
    with open(JSON_F, 'r') as fp:
        j = json.load(fp)
    with open(CBOR_F, 'wb') as fp:
        cbor2.dump(j, fp)

