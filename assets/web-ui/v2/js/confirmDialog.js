/* SPX - A seamless profiler for PHP
 * Copyright (C) 2017-2026 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

class ConfirmDialog {
    static #dialog = null;
    static #titleElement = null;
    static #messageElement = null;
    static #confirmButton = null;
    static #cancelButton = null;

    static #init() {
        if (this.#dialog) {
            return;
        }

        this.#dialog = document.createElement('dialog');
        this.#dialog.id = 'confirm-dialog';
        this.#dialog.innerHTML = `
            <div id="confirm-dialog-title"></div>
            <div id="confirm-dialog-message"></div>
            <div id="confirm-dialog-actions">
                <button id="confirm-dialog-cancel"></button><!--
                --><button id="confirm-dialog-confirm"></button>
            </div>
        `;
        document.body.appendChild(this.#dialog);

        this.#titleElement = this.#dialog.querySelector(
            '#confirm-dialog-title'
        );
        this.#messageElement = this.#dialog.querySelector(
            '#confirm-dialog-message'
        );
        this.#confirmButton = this.#dialog.querySelector(
            '#confirm-dialog-confirm'
        );
        this.#cancelButton = this.#dialog.querySelector(
            '#confirm-dialog-cancel'
        );
    }

    static confirm(title, message, confirmLabel, cancelLabel) {
        this.#init();

        return new Promise((resolve) => {
            this.#titleElement.textContent = title;
            this.#messageElement.innerHTML = message;
            this.#confirmButton.textContent = confirmLabel;
            this.#cancelButton.textContent = cancelLabel;
            this.#dialog.showModal();

            const cleanup = (value) => {
                this.#dialog.close();
                this.#confirmButton.removeEventListener('click', onConfirm);
                this.#cancelButton.removeEventListener('click', onCancel);
                this.#dialog.removeEventListener('cancel', onCancel);
                resolve(value);
            };

            const onConfirm = () => cleanup(true);
            const onCancel = (e) => {
                e.preventDefault();
                cleanup(false);
            };

            this.#confirmButton.addEventListener('click', onConfirm);
            this.#cancelButton.addEventListener('click', onCancel);
            this.#dialog.addEventListener('cancel', onCancel);
        });
    }
}

export function confirm(title, message, confirmLabel, cancelLabel) {
    return ConfirmDialog.confirm(title, message, confirmLabel, cancelLabel);
}
