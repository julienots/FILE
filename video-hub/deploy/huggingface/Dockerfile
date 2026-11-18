# Space Hugging Face (SDK Docker) : construit le hub depuis le dépôt GitHub public.
FROM python:3.12-slim
RUN apt-get update && apt-get install -y --no-install-recommends git ffmpeg && rm -rf /var/lib/apt/lists/*
ARG HUB_REPO=https://github.com/julienots/FILE
ARG HUB_BRANCH=claude/new-session-80zty9
# Change la valeur ci-dessous (ou « Factory rebuild ») pour récupérer la dernière version du code.
ARG HUB_REV=1
RUN git clone --depth 1 -b "$HUB_BRANCH" "$HUB_REPO" /src && cp -r /src/video-hub /app && rm -rf /src
WORKDIR /app
RUN pip install --no-cache-dir -r requirements.txt
# /data : stockage persistant du Space s'il est activé, sinon disque temporaire
RUN mkdir -p /data && chmod 777 /data
ENV HUB_DATA_DIR=/data
EXPOSE 7860
CMD ["uvicorn", "hub.main:app", "--host", "0.0.0.0", "--port", "7860", "--proxy-headers", "--forwarded-allow-ips", "*"]
