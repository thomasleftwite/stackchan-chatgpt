"""Voice admission, persisted selection, and local preview contracts. API0/USB0."""
import base64
import json
from pathlib import Path
import sys
import tempfile
import threading
import tkinter as tk
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from voices import VoiceReviews
from settings import save_voice_setting
from voice_preview import decode_audio,play_pcm
from host_app import HostApp,runtime_env

class VoiceFeatureTest(unittest.TestCase):
    def folder(self):
        parent=Path(__file__).resolve().parents[1]/'.pio/voice-tests';parent.mkdir(parents=True,exist_ok=True)
        return tempfile.TemporaryDirectory(dir=parent)
    def test_unreviewed_and_excluded_voices_are_not_normal_choices(self):
        with self.folder() as folder:
            r=VoiceReviews(Path(folder)/'voices.json');model='gemini-3.8-live'
            self.assertIn('Kore',r.candidates(model));self.assertEqual(r.approved(model),[])
            with self.assertRaises(ValueError):r.decide(model,'Kore',True,pc_confirmed=True)
            r.decide(model,'Kore',True,pc_confirmed=True,device_confirmed=True,app_version='test')
            self.assertEqual(VoiceReviews(r.path).approved(model),['Kore'])
            r.decide(model,'Kore',False,reason='発音が不自然');self.assertEqual(r.approved(model),[])
    def test_model_and_catalog_revision_require_new_review(self):
        with self.folder() as folder:
            r=VoiceReviews(Path(folder)/'v.json');r.decide('gemini-3.8-live','Kore',True,pc_confirmed=True,device_confirmed=True)
            self.assertEqual(r.approved('gemini-unsupported'),[])
            catalog=dict(r.catalog,revision='next');self.assertEqual(VoiceReviews(r.path,catalog=catalog).approved('gemini-3.8-live'),[])
    def test_corrupt_review_is_preserved_and_recoverable(self):
        with self.folder() as folder:
            p=Path(folder)/'v.json';p.write_text('{broken')
            r=VoiceReviews(p);self.assertTrue(r.load_error);self.assertEqual(r.approved('gemini-3.8-live'),[])
            r.decide('gemini-3.8-live','Kore',True,pc_confirmed=True,device_confirmed=True)
            self.assertEqual(next(Path(folder).glob('*.invalid-*')).read_text(),'{broken')
            self.assertEqual(VoiceReviews(p).approved('gemini-3.8-live'),['Kore'])
            p.write_text(json.dumps({'version':1,'records':{'gemini-3.8-live:Kore':None}}))
            self.assertTrue(VoiceReviews(p).load_error)
    def test_voice_only_save_retains_encrypted_key_and_network(self):
        with self.folder() as folder:
            p=Path(folder)/'settings.json';old={'version':1,'protectedKey':'opaque-encrypted-fixture','model':'gemini-3.8-live','host':'192.168.137.1'}
            p.write_text(json.dumps(old));save_voice_setting(p,'Kore');new=json.loads(p.read_text())
            self.assertEqual({k:new[k] for k in old},old);self.assertEqual(new['voice'],'Kore')
    def test_invalid_voice_cannot_modify_saved_settings(self):
        with self.folder() as folder:
            p=Path(folder)/'s.json'
            with self.assertRaises(ValueError):save_voice_setting(p,'../../secret')
            self.assertFalse(p.exists())
    def test_preview_rejects_non_pcm_and_cancelled_playback_does_not_open_audio(self):
        self.assertEqual(decode_audio({'pcm':base64.b64encode(b'\x01\x00').decode()}),bytearray(b'\x01\x00'))
        for value in ('!!!!',base64.b64encode(b'x').decode(),''):
            with self.assertRaises(ValueError):decode_audio({'pcm':value})
        stop=threading.Event();stop.set();self.assertFalse(play_pcm(bytearray(8),stop))
    def test_runtime_api_default_does_not_inherit_voice(self):
        import os
        from unittest.mock import patch
        with patch.dict(os.environ,{'GEMINI_VOICE':'Puck'}):
            self.assertNotIn('GEMINI_VOICE',runtime_env())
            self.assertEqual(runtime_env(voice='Kore')['GEMINI_VOICE'],'Kore')
    def test_ui_shows_only_approved_voices_and_has_no_pitch_or_speed(self):
        with self.folder() as folder:
            root=tk.Tk();root.withdraw()
            try:
                app=HostApp(root,discover=False,config_path=Path(folder)/'settings.json')
                self.assertEqual(str(app.voice_box.cget('state')),'readonly')
                self.assertEqual(tuple(app.voice_box['values']),())
                app.reviews.decide('gemini-3.8-live','Kore',True,pc_confirmed=True,device_confirmed=True)
                app.refresh_voices();self.assertEqual(tuple(app.voice_box['values']),('Kore',))
                app.voice.set('Kore');app.choose_voice()
                self.assertEqual(json.loads(app.path.read_text())['voice'],'Kore')
                app.session_busy=True;app.preview('Kore');self.assertIsNone(app.preview_job)
                app.preview_job={'id':1};app.start();self.assertIn('試聴を停止',app.preview_status.get());app.preview_job=None
                from unittest.mock import Mock
                app.worker=Mock();app.worker.is_alive.return_value=True;app.transport.set('USB診断（従来）')
                app.pc_heard.add(('gemini-3.8-live','Puck'));app.review_device('gemini-3.8-live','Puck')
                self.assertEqual(app.evaluation_voice,'');self.assertIn('USB診断を停止',app.preview_status.get())
            finally:root.destroy()
