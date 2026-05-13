from datetime import datetime

from intervaltree import IntervalTree

from neural.typing.sample_types import FFTLabels

def isoify(iso_ts: str):
    return int(datetime.fromisoformat(iso_ts).timestamp() * 1000)

class FFTLabelIndex:
    def __init__(self, recording: FFTLabels):
        self.tree = IntervalTree()

        for cls, ranges in recording["classes"].items():
            for r in ranges:
                self.tree.addi(isoify(r["start"]), isoify(r["end"]), cls)

    def __getitem__(self, key: int):
        return [x.data for x in self.tree.at(key)]


# if __name__ == "__main__":
#     print("bla")
#     labels: FFTLabels = {
#         "classes": {
#             "aku-bosh": [
#                 {
#                     "start": "2026-05-08T14:01:19.440+02:00",
#                     "end": "2026-05-08T14:01:22.070+02:00"
#                 },
#                 {
#                     "start": "2026-05-08T14:01:26.100+02:00",
#                     "end": "2026-05-08T14:01:32.600+02:00"
#                 },
#                 {
#                     "start": "2026-05-08T14:01:37.343+02:00",
#                     "end": "2026-05-08T14:01:47.109+02:00"
#                 },
#                 {
#                     "start": "2026-05-08T14:02:04.620+02:00",
#                     "end": "2026-05-08T14:02:08.300+02:00"
#                 }
#             ]
#         },
#         "recording": {
#             "filename": ""
#         }
#     }

#     index = FFTLabelIndex(labels)

#     print(index[isoify("2026-05-08T14:02:05.620+02:00")])
#     print(index[isoify("2026-05-08T14:02:07.620+02:00")])
#     print(index[isoify("2026-05-08T14:00:00.620+02:00")])
