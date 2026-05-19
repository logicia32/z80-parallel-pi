# graphs/ — 速度向上と精度のグラフを生成
#
#   make            ... speedup.{png,svg} / accuracy.{png,svg} を作る
#   make clean
#
# 必要: python3 + matplotlib。venv 等の別 python を使う場合:
#   make PYTHON=/path/to/python3
#
# plot.py に埋め込んだ数値は parallel/ と methods/ の harness を実際に
# 走らせて採った測定値(再現手順は plot.py 冒頭の docstring 参照)。
PYTHON ?= python3

all: speedup.png accuracy.png

speedup.png accuracy.png: plot.py
	$(PYTHON) plot.py

clean:
	rm -f speedup.png speedup.svg accuracy.png accuracy.svg

.PHONY: all clean
