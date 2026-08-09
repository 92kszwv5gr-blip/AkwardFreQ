"""
sound_forge/chopper/export/ableton_drum_rack_exporter.py
"""
import os
import gzip
import xml.etree.ElementTree as ET


def _make_drum_rack_xml(slices, instrument_name: str) -> bytes:
    ableton = ET.Element("Ableton", MajorVersion="11", MinorVersion="0.2")
    rack = ET.SubElement(ableton, "DrumGroupDevice", Id="1")
    ET.SubElement(rack, "DisplayName", Value=instrument_name)
    drum_branches = ET.SubElement(rack, "DrumBranches")

    for slc in slices:
        wav_name = os.path.basename(slc.filename) if slc.filename else f"slice_{slc.index:03d}.wav"
        branch = ET.SubElement(drum_branches, "DrumBranch", Id=str(slc.index))
        ET.SubElement(branch, "ReceivingNote", Value=str(slc.midi_note))
        ET.SubElement(branch, "Name", Value=wav_name.replace(".wav", "")[:16])
        devices = ET.SubElement(branch, "DeviceChain")
        simpler = ET.SubElement(devices, "OriginalSimpler", Id=str(slc.index))
        player = ET.SubElement(simpler, "Player")
        ET.SubElement(player, "SampleRef",
                      RelativePath=f"samples/{wav_name}",
                      DefaultDuration=str(slc.end_frame - slc.start_frame),
                      DefaultSampleRate=str(slc.sample_rate))

    return gzip.compress(ET.tostring(ableton, encoding="unicode").encode("utf-8"))


class AbletonDrumRackExporter:
    def export(self, slices, output_dir: str, instrument_name: str) -> str:
        os.makedirs(output_dir, exist_ok=True)
        os.makedirs(os.path.join(output_dir, "samples"), exist_ok=True)
        output_path = os.path.join(output_dir, f"{instrument_name}.adg")
        with open(output_path, "wb") as f:
            f.write(_make_drum_rack_xml(slices, instrument_name))
        return output_path
