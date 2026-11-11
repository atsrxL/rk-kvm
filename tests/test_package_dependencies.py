# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('package_server',ROOT/'tools/package_server.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DependencyPackagingTests(unittest.TestCase):
    def test_only_reachable_libraries_are_packaged(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder)
            for name in ['server','libopus.so.0','libasound.so.2','libold.so.1']:(d/name).touch()
            graph={'server':['libopus.so.0','libc.so.6'],'libopus.so.0':['libasound.so.2'],'libasound.so.2':[]}
            with patch.object(m,'needed',side_effect=lambda p:graph[p.name]):
                result=m.runtime_closure([d/'server'],d)
            self.assertEqual(set(result),{'libopus.so.0','libasound.so.2'})
    def test_forbidden_transitive_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);(d/'server').touch();(d/'libhelper.so').touch()
            with patch.object(m,'needed',side_effect=lambda p:['libhelper.so'] if p.name=='server' else ['libavcodec.so.61']):
                with self.assertRaises(ValueError):m.runtime_closure([d/'server'],d)
    def test_glib_is_allowed_but_graphics_and_legacy_media_are_rejected(self):
        self.assertFalse(m.forbidden_library('libglib-2.0.so.0'))
        for name in ['libGL.so.1','libEGL.so.1','libGLESv2.so.2','libcurl.so.4','libpulse.so.0','libminiupnpc.so.18']:
            self.assertTrue(m.forbidden_library(name))
