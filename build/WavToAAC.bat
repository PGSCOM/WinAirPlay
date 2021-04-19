REM ffmpeg -i ephemeral-rift-16-2-44100.wav -c:a aac -profile:a aac_eld -ac 2 ephemeral-rift-eld.mp4
REM ffmpeg -i ephemeral-rift-16-2-44100.wav -c:a aac -profile:a aac_low -ac 2 ephemeral-rift-lc.aac
ffmpeg -i ephemeral-rift-16-2-44100.wav -c:a aac_mf -ac 2 ephemeral-rift-lc.aac