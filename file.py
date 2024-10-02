import csv

# ファイル名を生成する範囲
start_num1 = 56833
end_num1 = 58367
start_num2 = 25600
end_num2 = 26111

# CSVファイルのパス
csv_file_path = "inputcp.csv"

# CSVファイルに書き出し
with open(csv_file_path, mode='w', newline='') as file:
    writer = csv.writer(file)
    
    # ファイルパスの生成と書き出し
    for num1 in range(start_num1, end_num1 + 1):
        for num2 in range(start_num2, end_num2 + 1):
            # "std/16/56320/25600.png" のような形式で生成
            file_path = f"std/16/{num1}/{num2}.png"
            # CSVに書き出し（1行に1つのパスを書き出し）
            writer.writerow([file_path])

print(f"{csv_file_path} に書き出しが完了しました。")
