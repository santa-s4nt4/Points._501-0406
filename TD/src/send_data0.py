"""
CHOP Execute DAT

me - this DAT

Make sure the corresponding toggle is enabled in the CHOP Execute DAT.
"""

# --- 設定 ---
# 実際のピクセルデータを持っているCHOPのパスを指定
# (前回作成した Math CHOP などを指定します)
DATA_SOURCE_CHOP = op('data0')

# データを送信するSerial DATのパスを指定
SERIAL_OP = op('serial1')

# Arduinoと合わせるヘッダー
HEADER = b'S'

def onOffToOn(channel, sampleIndex, val, prev):
	# データソースCHOPから全サンプルを取得
	samples = DATA_SOURCE_CHOP[0].vals

	# データの長さをチェック
	if len(samples) != 27:
		return

	# データをバイトに変換
	int_samples = [int(s) for s in samples]
	data_bytes = bytes(int_samples)
	message = HEADER + data_bytes

	# シリアル送信
	try:
		SERIAL_OP.sendBytes(message)
	except Exception as e:
		print(f"Serial Send Error: {e}")
	return