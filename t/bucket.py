#! /usr/bin/python --tt

import sys
import requests

def bucket_create(url):

    url = url + '/bucket/key1'
    payload = '{"name": "sujan dutta"}'

    headers = {'Content-Type':'application/json'}
    headers['Content-Length'] = len(payload);
    headers['Connection'] = 'close';

    res = requests.put(url, headers=headers, data=payload)
    print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))

def bucket_get(url):

    url += '/bucket/key1'
    headers = {'Connection': 'close'}
    res = requests.get(url, headers=headers)
    print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))
    if res.status_code == 200:
        print '{}:{} --> {}'.format(res.headers['Content-Length'], res.headers['Content-Type'], res.text)

    
def main(args):

    if len(args) != 1:
        print 'command missing'
        sys.exit(-1)

    baseurl = 'http://127.0.0.1:7878'

    if args[0] == 'c':
        r = bucket_create(baseurl)
    elif args[0] == 'g':
        r = bucket_get(baseurl)

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
