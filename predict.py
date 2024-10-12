import streamlit as st
from transformers import RobertaTokenizer, RobertaForSequenceClassification
import numpy as np
import torch
import os
import time

# Load the fine-tuned model and tokenizer
model_path = "model_13_datasets"

try:
    tokenizer = RobertaTokenizer.from_pretrained(model_path, use_fast=True)  # Use fast tokenizer
    model = RobertaForSequenceClassification.from_pretrained(model_path)
except Exception as e:
    st.error(f"Error loading model: {e}")
    st.stop()  # Stop the execution if model loading fails

# Define the programming language labels (in lowercase)
labels = ['cpp', 'json', 'java', 'javascript', 'groovy', 'python', 'xml', 'yml', 'sql', 'scala', 'go', 'php', 'swift']

# Function to predict programming languages for a batch of files
def predict_language_batch(file_contents_list):
    # Tokenize all file contents at once with smaller max_length if possible
    encoding = tokenizer(file_contents_list, truncation=True, padding=True, max_length=512, return_tensors="pt")

    # Check if GPU is available and set the device accordingly
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model.to(device)  # Move model to the appropriate device
    encoding = {k: v.to(device) for k, v in encoding.items()}  # Move inputs to the same device as the model

    model.eval()
    
    # Get the model predictions for all files
    with torch.no_grad():
        outputs = model(**encoding)
        logits = outputs.logits
        predicted_class_idx = torch.argmax(logits, dim=-1).tolist()

    # Debug: print predicted class indexes
    print("Predicted class indices:", predicted_class_idx)

    # Return the predicted languages in uppercase or 'UNKNOWN' for out of label cases
    return [
        labels[i].upper() if i < len(labels) else 'UNKNOWN' 
        for i in predicted_class_idx
    ]

# Function to read contents of a file (handling multiple file types including .yml)
def read_file_contents(file):
    file_extension = os.path.splitext(file.name)[1].lower()
    
    try:
        if file_extension in ['.txt', '.cpp', '.java', '.js', '.py', '.groovy', '.json', '.xml', '.yml', '.go', '.rb', '.scala', '.sql', '.swift', '.php', '.r']:
            return file.getvalue().decode("utf-8")
        else:
            return None
    except Exception as e:
        st.error(f"Error reading the file: {e}")
        return None

# Streamlit UI
def main():
    st.title("Programming Language and Configuration File Detector with CodeBERT")

    st.write("Upload one or more code or configuration files to detect their programming languages or file types using a fine-tuned CodeBERT model.")
    
    # File upload input - allowing multiple files including .yml
    uploaded_files = st.file_uploader("Choose files", type=["txt", "cpp", "java", "js", "py", "groovy", "json", "xml", "yml", 'go', 'ruby', 'scala', 'sql', 'swift', 'php'], accept_multiple_files=True)

    if uploaded_files:
        start_time = time.time()  # Track the time for performance

        file_contents_list = []
        file_names = []

        # Read the contents of each uploaded file
        for uploaded_file in uploaded_files:
            file_contents = read_file_contents(uploaded_file)
            if file_contents:
                file_contents_list.append(file_contents)
                file_names.append(uploaded_file.name)
            else:
                st.error(f"The file {uploaded_file.name} format is not supported or cannot be read.")

        # Predict the programming languages or file types for all files
        if file_contents_list:
            predicted_languages = predict_language_batch(file_contents_list)
            
            # Display results
            st.subheader("Predicted Programming Languages or File Types:")
            for file_name, language in zip(file_names, predicted_languages):
                st.write(f"{file_name}: {language}")

        # Ensure processing speed within 1 second for larger numbers of files
        total_time = time.time() - start_time
        if total_time > 1:
            st.warning(f"Processing took {total_time:.2f} seconds. This exceeds the 1 second goal.")

# Run the Streamlit app
if __name__ == "__main__":
    main()
