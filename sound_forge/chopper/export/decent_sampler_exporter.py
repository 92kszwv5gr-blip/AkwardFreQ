"""
sound_forge/chopper/export/decent_sampler_exporter.py
"""
import os
import xml.etree.ElementTree as ET
from xml.dom import minidom


class DecentSamplerExporter:
    def export(self, slices: list, output_dir: str, instrument_name: str) -> str:
        os.makedirs(output_dir, exist_ok=True)
        os.makedirs(os.path.join(output_dir, "samples"), exist_ok=True)

        root = ET.Element("DecentSampler", minVersion="1.0.0")
        ET.SubElement(root, "ui", width="812", height="375")
        groups = ET.SubElement(root, "groups")
        group = ET.SubElement(groups, "group")

        for slc in slices:
            wav_name = os.path.basename(slc.filename) if slc.filename else f"slice_{slc.index:03d}.wav"
            ET.SubElement(group, "sample",
                path=f"samples/{wav_name}",
                rootNote=str(slc.midi_note),
                loNote=str(slc.midi_note),
                hiNote=str(slc.midi_note),
                loVel="0", hiVel="127", volume="1.0")

        output_path = os.path.join(output_dir, f"{instrument_name}.dspreset")
        xml_str = minidom.parseString(ET.tostring(root)).toprettyxml(indent="  ")
        with open(output_path, "w") as f:
            f.write(xml_str)
        return output_path
