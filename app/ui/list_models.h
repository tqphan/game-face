#pragma once

#include <game_face/core/binding_engine.h>
#include <game_face/core/blendshapes.h>
#include <game_face/core/profiles.h>

#include <QAbstractListModel>
#include <QQmlEngine>

#include <span>
#include <vector>

// Names of the profiles in a ProfileSet owned by AppController.
class ProfileListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1 };

    explicit ProfileListModel(game_face::ProfileSet* set, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return rowCount(); }

    void reload();

signals:
    void countChanged();

private:
    game_face::ProfileSet* set_;
};

// The bindings of the selected profile, editable from QML through roles
// (e.g. `model.threshold = 40` in a delegate). Also carries validation errors
// and live tracking state for display.
class BindingListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        BindingEnabledRole,
        SimplifiedRole,
        BlendshapeRole,
        ThresholdRole,
        SimpleStartCommandRole,
        SimpleStopCommandRole,
        StartExpressionRole,
        StartCommandRole,
        StartDebounceRole,
        StopExpressionRole,
        StopCommandRole,
        StopDebounceRole,
        // read-only
        StartExpressionErrorRole,
        StopExpressionErrorRole,
        SimpleStartCommandErrorRole,
        SimpleStopCommandErrorRole,
        StartCommandErrorRole,
        StopCommandErrorRole,
        StartActiveRole,
        StopActiveRole,
        SimpleActiveRole,
        LiveValueRole,
    };

    explicit BindingListModel(game_face::ProfileSet* set, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return rowCount(); }

    // After the selection or the profile's binding list changed.
    void reload();
    // Per-frame state from the tracker; emits only for rows that changed.
    void setLiveState(std::span<const game_face::BindingStatus> status,
                      const game_face::BlendshapeScores& scores);
    void clearLiveState();

signals:
    void countChanged();
    // A binding was edited by the user.
    void edited();

private:
    struct RowState {
        QString start_expression_error;
        QString stop_expression_error;
        QString simple_start_command_error;
        QString simple_stop_command_error;
        QString start_command_error;
        QString stop_command_error;
        bool start_active = false;
        bool stop_active = true;
        bool simple_active = false;
        double live_value = 0;
    };

    game_face::Profile* profile() const;
    void validate(int row);

    game_face::ProfileSet* set_;
    std::vector<RowState> rows_;
};

// The 52 blendshape scores (0..100), for the Blendshapes dialog.
class BlendshapeModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum Role { NameRole = Qt::UserRole + 1, ValueRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setScores(const game_face::BlendshapeScores& scores);

private:
    game_face::BlendshapeScores scores_{};
};
