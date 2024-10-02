import os
import csv
import requests
from concurrent.futures import ThreadPoolExecutor, as_completed

# CSVファイルのパス
csv_file = 'input.csv'

# ベースURL
base_url = "https://cyberjapandata.gsi.go.jp/xyz/"

# 保存するベースディレクトリ
save_base_dir = "images"

# 同時に処理するスレッドの数
NUM_THREADS = 10

# ダウンロードを行う関数
def download_image(row):
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
        return

    # ダウンロードURLの生成
    download_url = base_url + row[0]
    
    try:
        print(f"Downloading {download_url}")
        response = requests.get(download_url)

        # ダウンロードが成功した場合
        if response.status_code == 200:
            with open(save_path, 'wb') as img_file:
                img_file.write(response.content)
            print(f"Downloaded {file_name}")
        else:
            print(f"Failed to download {download_url}, status code: {response.status_code}")
    except requests.exceptions.ConnectionError as e:
        print(f"Connection error for {download_url}: {e}")
    except Exception as e:
        print(f"An error occurred for {download_url}: {e}")

# CSVを読み込んでリスト化
with open(csv_file, newline='', encoding='utf-8') as f:
    reader = list(csv.reader(f))  # CSV全体をリストとして読み込む

    # スレッドプールを使って並列ダウンロード
    with ThreadPoolExecutor(max_workers=NUM_THREADS) as executor:
        futures = [executor.submit(download_image, row) for row in reader if row[0].startswith('std/16')]
        
        for future in as_completed(futures):
            future.result()  # 例外が発生した場合にはここでキャッチ
