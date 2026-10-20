import json
from pathlib import Path
import tempfile
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from wireless import normalize_code,save_pairing,load_pairing,runtime_node,run_wireless,wireless_status

class WirelessHostTests(unittest.TestCase):
 def test_failure_status_reaches_ui_without_provider_text_or_credentials(self):
  from host_app import HostApp
  event={'event':'turn-failed','code':'QUOTA','status':429,'message':'PRIVATE_PROVIDER_TEXT','apiKey':'PRIVATE_KEY'}
  status=wireless_status(event)
  self.assertEqual(status,'WIRELESS turn-failed QUOTA 429')
  friendly=HostApp.friendly(None,status)
  self.assertIn('利用制限',friendly);self.assertIn('429',friendly)
  self.assertNotIn('PRIVATE',status+friendly)
 def test_failure_status_rejects_unknown_codes_and_invalid_http_status(self):
  self.assertEqual(wireless_status({'event':'turn-failed','code':'PRIVATE_KEY','status':429}),'WIRELESS turn-failed')
  self.assertEqual(wireless_status({'event':'turn-failed','code':{'message':'PRIVATE_KEY'},'status':429}),'WIRELESS turn-failed')
  for status in ('429',True,99,600):
   self.assertEqual(wireless_status({'event':'turn-failed','code':'AUTH','status':status}),'WIRELESS turn-failed AUTH 0')
  self.assertIsNone(wireless_status({'event':'PRIVATE_PROVIDER_TEXT'}))
  self.assertEqual(wireless_status({'event':'device-online'}),'WIRELESS device-online')
 def folder(self):
  root=Path(__file__).resolve().parents[1]/'.pio/wireless-tests';root.mkdir(parents=True,exist_ok=True)
  return tempfile.TemporaryDirectory(dir=root)
 def test_pairing_code_requires_key_length_and_accepts_display_line_breaks(self):
  self.assertEqual(normalize_code('12345678 90ABCDEF\n12345678 90ABCDEF'),'1234567890abcdef1234567890abcdef')
  for code in ('12345678','x'*32,'g'*32,'a'*33):
   with self.assertRaises(ValueError):normalize_code(code)
 def test_pairing_is_persistent_encrypted_and_corruption_fails_closed(self):
  with self.folder() as folder:
   path=Path(folder)/'pairing.json';code='1234567890abcdef1234567890abcdef'
   save_pairing(path,code);self.assertNotIn(code,path.read_text());self.assertEqual(load_pairing(path),code)
   data=json.loads(path.read_text());data['protectedDeviceKey']='AAAA';path.write_text(json.dumps(data))
   with self.assertRaises(Exception):load_pairing(path)
 def test_stable_runtime_repairs_modified_executable(self):
  with self.folder() as folder:
   root=Path(folder);source=root/'bundle';source.mkdir();(source/'node.exe').write_bytes(b'fixture-runtime')
   target=runtime_node(source,root/'installed');target.write_bytes(b'tampered')
   self.assertEqual(runtime_node(source,root/'installed'),target);self.assertEqual(target.read_bytes(),b'fixture-runtime')
 def test_stale_interface_is_rejected_before_server_or_usb_access(self):
  result=[]
  with patch('network.local_addresses',return_value=['192.168.137.1']),patch('wireless.subprocess.Popen') as child:
   self.assertEqual(run_wireless('.','unused','192.168.1.5','a'*32,{},None,result.append),1)
   child.assert_not_called()
  self.assertEqual(result,['WIRELESS stale_address'])
