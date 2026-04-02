#!/usr/bin/env bash
set -e

ENV_FILE="$HOME/.cpp_ai_service.env"
PROJECT_DIR="$HOME/CppAIService"
BUILD_DIR="$PROJECT_DIR/build"
PORT=8080

if [ ! -f "$ENV_FILE" ]; then
  echo "环境变量文件不存在: $ENV_FILE"
  exit 1
fi

set -a
source "$ENV_FILE"
set +a

cd "$BUILD_DIR"

exec ./http_server -p "$PORT"