import requests
import json

# Payload + URL
url = "http://104.230.97.51:25570/api/generate" # Public
# url = "http://192.168.50.149:25570/api/generate" # Local
#question = input("Question: ")
question = "What is your purpose"
payload = {
    "model": "mistral",
    "prompt": question,
    "stream": False,
}

# Get Response
response = requests.post(url, data=json.dumps(payload))
response_json = response.json()

# Extract and print the "response" section
response_text = response_json.get("response")
print(response_text)