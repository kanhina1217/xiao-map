import os
import csv
import requests
from PIL import Image
from io import BytesIO
from time import sleep

# CSVファイルのパス
csv_file = 'inputcp.csv'

# ベースURL
base_url = "https://cyberjapandata.gsi.go.jp/xyz/"

# 保存するベースディレクトリ
save_base_dir = "images"

# 一色かどうかを確認するための指定色 (例: 白色)
specified_color = (255, 255, 255)  # RGB値

# 画像が指定色一色か確認する関数
def is_image_one_color(image, color):
    # 画像のピクセルデータを取得
    pixels = list(image.getdata())
    # すべてのピクセルが指定した色と一致するか確認
    return all(p == color for p in pixels)

# リトライ設定
MAX_RETRIES = 5
RETRY_DELAY = 5  # リトライ間隔（秒）

# CSVからstd/16で始まる行を抽出して処理
with open(csv_file, newline='', encoding='utf-8') as f:
    reader = csv.reader(f)
    
    for row in reader:
        # 行の先頭が'std/16'で始まっているか確認
        if row[0].startswith('std/16'):
            # ディレクトリとファイル名を分割
            dir_name, file_name = row[0].rsplit('/', 1)
            
            # 保存するディレクトリを作成
            save_dir = os.path.join(save_base_dir, dir_name)
            os.makedirs(save_dir, exist_ok=True)
            
            # 保存するファイルパス
            save_path = os.path.join(save_dir, file_name)
            
            # ファイルがすでに存在するか確認
            if os.path.exists(save_path):
                print(f"File already exists: {save_path}, skipping download.")
                continue  # ファイルが存在する場合は次の行へスキップ
            
            # ダウンロードURLの生成
            download_url = base_url + row[0]
            
            # リトライ処理を追加
            for attempt in range(MAX_RETRIES):
                try:
                    print(f"Downloading {download_url}, attempt {attempt + 1}")
                    response = requests.get(download_url)
                    
                    # ダウンロードが成功した場合
                    if response.status_code == 200:
                        # 画像データを一時的にメモリに保持
                        image = Image.open(BytesIO(response.content))
                        
                        # 画像が指定した色一色で塗りつぶされているか確認
                        if is_image_one_color(image, specified_color):
                            print(f"Skipping download of {file_name}: image is one color.")
                        else:
                            # 一色ではない場合、ファイルに保存
                            with open(save_path, 'wb') as img_file:
                                img_file.write(response.content)
                            print(f"Downloaded {file_name}")
                        break  # 成功したのでリトライループを終了
                    else:
                        print(f"Failed to download {download_url}, status code: {response.status_code}")
                        break  # HTTPエラーの場合はリトライせず終了
                
                except requests.exceptions.ConnectionError as e:
                    print(f"Connection error: {e}, retrying in {RETRY_DELAY} seconds...")
                    sleep(RETRY_DELAY)  # リトライ前に待機
                
                except Exception as e:
                    print(f"An error occurred: {e}")
                    break  # その他のエラーが発生した場合はリトライせず終了
