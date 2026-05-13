
from itertools import chain
import os
import signal

import requests


from torch.utils.data import DataLoader, Sampler
import torch

from typing import Callable, Iterable


from torch import nn, triplet_margin_loss
from torch.optim import Optimizer
from torch.amp.grad_scaler import GradScaler

import torch.nn.functional as F
from torch.nn import TripletMarginLoss

import json
from pathlib import Path
from torchvision.utils import save_image

from neural.dataset.fft_samples_dataset import FFTSampleDataset
from neural.models.fft_classifier import FFTClassifier
from neural.neural_constants import CURRENT_WEIGHTS_DIR, DATA_RECORD_TRN, DATA_RECORD_VAL, NUM_FFT_MODEL, SIGNAL_CLASSES
from neural.typing.sample_types import FFTTensorData


loss_fn = torch.nn.BCEWithLogitsLoss()

def train_one_epoch(
    model: nn.Module,
    loader: DataLoader,
    optimizer: Optimizer,
    device: torch.device,
    scaler: GradScaler,
) -> float:

    model.train()
    total_loss: float = 0.0
    num_batches: int = 0
    batch: FFTTensorData
    for batch in loader:

        
        optimizer.zero_grad(set_to_none=True)

        with torch.autocast(device_type=device.type, enabled=False):
            pred_class: torch.Tensor = model(batch["samples"].to(device))

            loss: torch.Tensor = loss_fn(pred_class, batch["signal_classes"].to(device))

        scaler.scale(loss).backward()
        scaler.step(optimizer)
        scaler.update()

        curr_loss = float(loss.detach().cpu())
        total_loss += curr_loss

        if num_batches % 5 == 0:
            print(f"    Batch {num_batches: >5} Loss: {curr_loss:.5f}")
            class_means = batch["signal_classes"].mean(dim=0)          # [NUM_CLASSES]

            print("    Per-class mean probabilities:")
            for idx, val in enumerate(class_means):
                print(f"      class {idx} {SIGNAL_CLASSES[idx]}: {val.item():.4f}")

        num_batches += 1

    return total_loss / max(1, num_batches)

def evaluate(
    model: nn.Module,
    loader: DataLoader,
    device: torch.device,
) -> float:
    """
    Runs one evaluation epoch (no gradients, no optimizer, no scaler).

    Args:
        model: The neural network being evaluated.
        loader: DataLoader providing validation batches.
        device: CUDA or CPU device.

    Returns:
        Average loss over the epoch.
    """
    model.eval()
    total_loss: float = 0.0
    num_batches: int = 0

    with torch.no_grad():
        batch: FFTTensorData
        for batch in loader:
            pred_class: torch.Tensor = model(batch["samples"].to(device))

            loss: torch.Tensor = loss_fn(pred_class, batch["signal_classes"].to(device))

            total_loss += float(loss.detach().cpu())
            num_batches += 1

    return total_loss / max(1, num_batches)


def train():

    # Dataset initialization
    train_dataset_src = FFTSampleDataset(
        DATA_RECORD_TRN,
        8,
        shuffle_seed=42,
        cache_folder="D:\\torch_data\\cached_fft_detect",
    )

    # print("Total samples: "+str(len(train_dataset_src)))
    # return

    val_dataset_src = FFTSampleDataset(
        DATA_RECORD_VAL,
        8,
        shuffle_seed=13,
        cache_folder="D:\\torch_data\\cached_fft_detect",
    )

    # print("Total val samples: "+str(len(val_dataset_src)))

    train_loader = DataLoader(
        train_dataset_src,
        batch_size=64,
        shuffle=False,
        num_workers=6,
        pin_memory=True,
        persistent_workers=True,
        prefetch_factor=4
    )

    val_loader = DataLoader(
        val_dataset_src,
        batch_size=16,
        shuffle=False,
        num_workers=2,
        pin_memory=True,
        persistent_workers=True,
        prefetch_factor=2
    )

    # Model initialization
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    if not torch.cuda.is_available():
        print("WARNING: training on CPU!")


    model = FFTClassifier(NUM_FFT_MODEL, 4096, class_names=SIGNAL_CLASSES).to(device=device)

    # print(f"Lin in: {model.flat_out_width}")
    # return
    
    num_epochs = 1000
    trainable_params = [p for p in model.parameters() if p.requires_grad]

    LR_DEFAULT = 1e-6
    LR_MIN = 1e-8
    lr_cur = LR_DEFAULT
    optimizer = torch.optim.AdamW(
        trainable_params,
        lr=LR_DEFAULT,
        betas=(0.95, 0.999),
        weight_decay=1e-13,
    )

    scaler = torch.amp.grad_scaler.GradScaler(device.type)   # create ONCE per epoch (or once per training run)

    # add_nan_hooks(model)
    os.makedirs(CURRENT_WEIGHTS_DIR, exist_ok=True)
    found_anything = False
    found_modules, needed_modules = model.load_weights(CURRENT_WEIGHTS_DIR)
    found_anything = found_modules > 0
    found_all = found_modules == needed_modules
    
    min_val_loss = 500.0
    learn_loss = 0.0

    interrupted = False

    def handle_interrupt(signum, frame):
        nonlocal interrupted
        interrupted = True
        # trainer.stopped = True
        print("Interrupt received. Will stop after current batch...")
    signal.signal(signal.SIGINT, handle_interrupt)
    # first_item = next(iter(train_loader))

    def change_lr(opt: torch.optim.Optimizer, val: float):
        for param_group in opt.param_groups:
            param_group['lr'] = val


    no_improvement_steps: int = 0
    improvement_steps: int = 0

    increase_memory: int = 0

    print("Started training.")
    for epoch in range(num_epochs):
        if epoch == 0:
            print("First epoch")

        # if epoch == 0:
        #     with torch.no_grad():
        #         for p in model.parameters():
        #             p.add_(0.005 * torch.randn_like(p))

        learn_loss = train_one_epoch(
            device=device, model=model, loader=train_loader, optimizer=optimizer, scaler=scaler
            )
        val_loss = 0.0
        val_loss_valid = False
        

        if not interrupted:
            val_loss = evaluate(device=device, model=model, loader=val_loader)
            print(f"Epoch {epoch+1}/{num_epochs} - val_loss: {val_loss:.4f}, lrn_loss: {learn_loss:.4f}")
            val_loss_valid = True
 
        if interrupted:
            break

        if val_loss_valid:
            if (val_loss < min_val_loss or min_val_loss == 500.0):
                print(f"  Loss improved, saving sample {min_val_loss:.4f} -> {val_loss:.4f}")

                min_val_loss = min(val_loss, min_val_loss)
                no_improvement_steps = 0
                improvement_steps += 1
                if lr_cur < LR_DEFAULT / 2.0 and improvement_steps > 10:
                    lr_cur *= 1.05
                    print("  Increased LR back up")
                    if lr_cur > LR_DEFAULT / 2.0:
                        lr_cur = LR_DEFAULT / 2.0
                    change_lr(optimizer, lr_cur)
                if epoch > 1 or not found_anything:
                    model.save_weights(CURRENT_WEIGHTS_DIR)
                    pass
            else:
                no_improvement_steps += 1
                improvement_steps = 0
                increase_memory += 1
                if no_improvement_steps > 3:
                    new_lr = lr_cur * 0.8
                    if new_lr < LR_MIN:
                        print(f"  Cannot find a better solution after {no_improvement_steps} steps")
                        break
                    change_lr(optimizer, new_lr)
                    lr_cur = new_lr
        if interrupted:
            break


# -------------------------------------------------------------------
# Script entry point
# -------------------------------------------------------------------

if __name__ == "__main__":
    train()