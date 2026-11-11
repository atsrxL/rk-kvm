# SPDX-License-Identifier: GPL-3.0-or-later
"""Transformation unit tests on small source fixtures; NOT full Sunshine patch application."""
import importlib.util
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile
p=Path(__file__).resolve().parents[1]/'tools/apply_sunshine.py'
spec=importlib.util.spec_from_file_location('apply_sunshine',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sys.path.insert(0,str(p.parent))
import prepare_sunshine as preparation
import audit_server_config as configuration_audit
class OverlayTests(unittest.TestCase):
    def test_preparation_is_read_only_and_headers_are_isolated(self):
        repo=p.parents[1]/'vendor/sunshine'
        before=subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True)
        if before.strip():self.skipTest('clean pinned upstream fixture required for read-only copy integration')
        with tempfile.TemporaryDirectory() as folder:
            out=Path(folder)/'protocol'
            preparation.prepare(repo,out)
            self.assertTrue((out/'src/config.h').is_file())
            self.assertNotIn('libavcodec',(out/'src/video.h').read_text())
            self.assertFalse((out/'src/video.cpp').exists())
            self.assertFalse((out/'CMakeLists.txt').exists())
            self.assertEqual(preparation.prepare(repo,out)['operation'],'reused')
            (out/'src/config.h').write_text('operator edit')
            with self.assertRaises(preparation.PatchError):preparation.prepare(repo,out,True)
        self.assertEqual(before,subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True))
    def test_configuration_keys_match_the_pinned_parser(self):
        result=configuration_audit.audit(p.parents[1]/'vendor/sunshine')
        self.assertEqual(result['unexpected_keys'],[])
        self.assertIn('stream_audio',result['replacement_keys'])
    def test_once(self):self.assertEqual(m.once('abc','b','x'),'axc')
    def test_missing_anchor(self):
        with self.assertRaises(m.PatchError):m.once('abc','q','x')
    def test_duplicate_anchor(self):
        with self.assertRaises(m.PatchError):m.once('aa','a','x')
    def test_multiline_signature(self):
        s='  void capture(\n    safe::mail_t mail,\n    config_t config,\n    void *channel_data\n  ) {\n    original();\n  }\n'
        r=m.at_entry(s,'  void capture(safe::mail_t mail, config_t config, void *channel_data)','    hook();\n')
        self.assertLess(r.index('hook();'),r.index('original();'))
    def test_changed_signature(self):
        with self.assertRaises(m.PatchError):m.at_entry('int f(int x) {\n}', 'int f(double x)','hook();')
    def test_queue_not_replaced(self):
        anchor='    explicit queue_t(std::uint32_t max_elements = 32, overflow_policy_e overflow = overflow_policy_e::drop_oldest):'
        r=m.make_changes({'src/thread_safe.h':anchor})['src/thread_safe.h']
        self.assertIn('rkmoon_try_raise',r);self.assertIn('_queue.size() >= limit',r);self.assertNotIn('_queue.clear()',r)
    def test_rtsp_admission_precedes_allocation(self):
        s='// local includes\n  void print_msg(PRTSP_MESSAGE msg) {\n}\n          BOOST_LOG(debug) << "Found Host: "sv << content;\n    auto stream_session = stream::session::alloc(config, session);\n'
        r=m.make_changes({'src/rtsp.cpp':s})['src/rtsp.cpp']
        self.assertLess(r.index('session_count()'),r.index('stream::session::alloc'))
        self.assertIn('453',r)
    def test_commit_fixed(self):self.assertEqual(m.PIN,'63d35f702ee9e362e43263742981836ec0710384')
