import os
import csv
import requests
from PIL import Image
from io import BytesIO

# CSVファイルのパス
csv_file = 'input.csv'

# ベースURL
base_url = "https://cyberjapandata.gsi.go.jp/xyz/"

# 保存するベースディレクトリ
save_base_dir = "images"

# 一色かどうかを確認するための指定色 (例: 白色)
specified_color = (190, 210, 255)  # RGB値

# 画像が指定色一色か確認する関数
def is_image_one_color(image, color):
    # 画像のピクセルデータを取得
    pixels = list(image.getdata())
    # すべてのピクセルが指定した色と一致するか確認
    return all(p == color for p in pixels)

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
            
            # ダウンロードURLの生成
            download_url = base_url + row[0]
            
            # 画像をダウンロード
            print(f"Downloading {download_url}")
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
            else:
                print(f"Failed to download {download_url}")
