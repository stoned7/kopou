#! /usr/bin/python --tt

import sys
import requests
from requests import Request, Session

def bucket_create_image(url):

    with open('/home/sujan/Virtualization.pdf') as fh:
        payload = fh.read()
        url = url + '/bucket/key91?abc=zyx'

        s = Session()
        headers = {'Content-Type':'application/pdf'}
        headers['Connection'] = 'close';

        req = Request('PUT', url, data=payload, headers=headers)
        prepped = req.prepare()

        res = s.send(prepped)    
        print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))

def bucket_create_json(url):

    with open('/home/sujan/test2.json') as fh:
        payload = fh.read()
        url = url + '/bucket/key92'

        s = Session()
        headers = {'Content-Type':'application/json'}
        headers['Connection'] = 'close';

        req = Request('PUT', url, data=payload, headers=headers)
        prepped = req.prepare()

        res = s.send(prepped)    
        print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))

def bucket_create_bulk(baseurl):

    for i in range(0, 1000000):
        url = baseurl + '/bucket/_codedeploplyment_user_key' + str(i)
        payload = '{"name": "sujan dutta' + str(i) + '", "district": "sibsagar' + str(i) + '"}'
        headers = {'Content-Type':'Application/Json'}
        headers['Content-Length'] = len(payload);
        headers['Connection'] = 'close';
        res = requests.put(url, headers=headers, data=payload)
        print '{}: {} {}, {}'.format(i, res.status_code, res.reason, str(res.elapsed))
        url = ''
        


def bucket_create(url):

    url = url + '/bucket/key1'
    payload = '{"name": "sujan dutta", "district": "sibsagar"}'

    headers = {'Content-Type':'Application/Json'}
    headers['Content-Length'] = len(payload);
    headers['Connection'] = 'close';

    res = requests.put(url, headers=headers, data=payload)
    print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))

def bucket_get(url):

    url += '/bucket/key2.jpg'
    headers = {'Connection': 'close'}
    res = requests.get(url, headers=headers)
    print '{} {}, {}'.format(res.status_code, res.reason, str(res.elapsed))
    if res.status_code == 200:
        print '{}:{}'.format(res.headers['Content-Length'], res.headers['Content-Type'])

    
def main(args):

    if len(args) != 1:
        print 'command missing'
        sys.exit(-1)

    baseurl = 'http://127.0.0.1:7878'

    if args[0] == 'c':
        bucket_create(baseurl)
    elif args[0] == 'g':
        bucket_get(baseurl)
    elif args[0] == 'i':
        bucket_create_image(baseurl)
        bucket_create_json(baseurl)
    elif args[0] == 'b':
        bucket_create_bulk(baseurl)

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
