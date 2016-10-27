import QtQuick 2.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.1
import QtQuick.Dialogs 1.2

ColumnLayout {

    state: "edit"

    states: [
        State { name: "new"},
        State { name: "add"},
        State { name: "edit"}
    ]

    function isValueChanged() {
        console.exception("Not implemented")
    }

    function resetAndDisableEditor() {
        console.exception("Not implemented")
    }

    function setValue(value) {
        console.exception("Not implemented")
    }

    function getValue() {
        console.exception("Not implemented")
    }

    function isValueValid() {
        console.exception("Not implemented")
    }

    function markInvalidFields() {
        console.exception("Not implemented")
    }

    function reset() {
        console.exception("Not implemented")
    }
}
