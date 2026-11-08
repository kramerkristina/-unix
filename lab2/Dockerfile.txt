FROM alpine:3.20

WORKDIR /app
COPY run.sh .

ENTRYPOINT ["sh", "run.sh"]
